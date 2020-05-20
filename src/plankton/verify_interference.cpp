#include "plankton/verify.hpp"

#include "cola/util.hpp"
#include "cola/visitors.hpp"
#include "plankton/config.hpp"

using namespace cola;
using namespace plankton;


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct EffectSearcher : public BaseVisitor {
	bool result = false;

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }

	void visit(const VariableExpression& node) override { result |= node.decl.is_shared; }
	void visit(const Dereference& /*node*/) override { result = true; }
	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
};

bool Verifier::has_effect(const Expression& assignee) {
	// TODO: filter out assignments that do not change the assignee valuation?
	EffectSearcher searcher;
	assignee.accept(searcher);
	return searcher.result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct InterferneceExpressionRenamer : public BaseNonConstVisitor {
	RenamingInfo& info;
	std::unique_ptr<Expression>* current_owner = nullptr;

	InterferneceExpressionRenamer(RenamingInfo& info_) : info(info_) {}

	void handle_expression(std::unique_ptr<Expression>& expr) {
		current_owner = &expr;
		expr->accept(*this);
	}

	void visit(VariableExpression& expr) override {
		if (expr.decl.is_shared) return;
		const VariableDeclaration* replacement;

		auto find = info.variable2renamed.find(&expr.decl);
		if (find != info.variable2renamed.end()) {
			// use existing replacement
			replacement = find->second;

		} else {
			// create new replacement
			auto decl = std::make_unique<VariableDeclaration>(expr.decl.name, expr.decl.type, expr.decl.is_shared);
			info.variable2renamed[&expr.decl] = decl.get();
			replacement = decl.get();
			info.renamed_variables.push_back(std::move(decl));
		}

		assert(replacement);
		assert(current_owner);
		assert(current_owner->get() == &expr);
		*current_owner = std::make_unique<VariableExpression>(*replacement);
	}
	
	void visit(Dereference& expr) override {
		handle_expression(expr.expr);
	}
	
	void visit(NegatedExpression& expr) override {
		handle_expression(expr.expr);
	}

	void visit(BinaryExpression& expr) override {
		handle_expression(expr.lhs);
		handle_expression(expr.rhs);
	}

	void visit(BooleanValue& /*expr*/) override { /* do nothing */ }
	void visit(NullValue& /*expr*/) override { /* do nothing */ }
	void visit(EmptyValue& /*expr*/) override { /* do nothing */ }
	void visit(MaxValue& /*expr*/) override { /* do nothing */ }
	void visit(MinValue& /*expr*/) override { /* do nothing */ }
	void visit(NDetValue& /*expr*/) override { /* do nothing */ }
};

struct BasicExtractor : public FormulaVisitor {
	std::unique_ptr<BasicFormula> result;
	InterferneceExpressionRenamer expression_renamer;

	BasicExtractor(RenamingInfo& info) : expression_renamer(info) {}

	template<typename C>
	void handle_conjunction(const C& formula) {
		auto form = std::make_unique<BasicConjunctionFormula>();
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
			if (result) {
				form->conjuncts.push_back(std::move(result));
			}
		}
		result.reset();
		if (form->conjuncts.size() != 0) {
			result = std::move(form);
		}
	}

	void visit(const ConjunctionFormula& formula) override {
		handle_conjunction(formula);
	}

	void visit(const BasicConjunctionFormula& formula) override {
		handle_conjunction(formula);
	}

	void visit(const ExpressionFormula& formula) override {
		auto expr = cola::copy(*formula.expr);
		expression_renamer.handle_expression(expr);
		result = std::make_unique<ExpressionFormula>(std::move(expr));
	}

	void visit(const HistoryFormula& /*formula*/) override {
		result.reset();
	}

	void visit(const FutureFormula& /*formula*/) override {
		result.reset();
	}
};

std::unique_ptr<BasicFormula> extract_base_formula(const Formula& formula, RenamingInfo& info) {
	BasicExtractor extractor(info);
	formula.accept(extractor);
	if (extractor.result) {
		return std::move(extractor.result);
	} else {
		return std::make_unique<ExpressionFormula>(std::make_unique<BooleanValue>(true));
	}
}

void Verifier::extend_interference(const Command& command) {
	auto pre = extract_base_formula(*current_annotation, interference_renaming_info);

	bool is_effect_new = true;
	bool effect_pruned = false;

	// check if we already covered the new effect; delete effects covered by the new one
	for (auto& other : interference) {
		if (&command != &other->command) {
			continue;
		}

		if (plankton::implies(*pre, *other->precondition)) {
			is_effect_new = false;
			break;
		}
		if (plankton::implies(*other->precondition, *pre)) {
			other.reset();
			effect_pruned = true;
		}
	}

	// remove pruned effects
	std::deque<std::unique_ptr<Effect>> new_interference;
	for (auto& other : interference) {
		if (other) {
			new_interference.push_back(std::move(other));
		}
	}
	interference = std::move(new_interference);

	// add new effect if needed
	if (is_effect_new) {
		interference.emplace_back(std::make_unique<Effect>(std::move(pre), command));
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct Splitter : public FormulaNonConstVisitor {
	std::unique_ptr<BasicConjunctionFormula> basic = std::make_unique<BasicConjunctionFormula>();
	std::unique_ptr<ConjunctionFormula> preds = std::make_unique<ConjunctionFormula>();

	template<typename C>
	void handle_conjunction(C& conjunction) {
		for (auto& conjunct : conjunction.conjuncts) {
			conjunct->accept(*this);
		}
	}

	void visit(ConjunctionFormula& formula) override {
		handle_conjunction(formula);
	}

	void visit(BasicConjunctionFormula& formula) override {
		handle_conjunction(formula);
	}

	void visit(ExpressionFormula& formula) override {
		basic->conjuncts.push_back(std::make_unique<ExpressionFormula>(std::move(formula.expr)));
	}

	void visit(HistoryFormula& formula) override {
		preds->conjuncts.push_back(std::make_unique<HistoryFormula>(std::move(formula.condition), std::move(formula.expression)));
	}

	void visit(FutureFormula& formula) override {
		preds->conjuncts.push_back(std::make_unique<FutureFormula>(std::move(formula.condition), formula.command));
	}
};

std::pair<std::unique_ptr<BasicConjunctionFormula>, std::unique_ptr<ConjunctionFormula>> split_annotation(std::unique_ptr<Formula> formula) {
	Splitter splitter;
	formula->accept(splitter);
	return std::make_pair(std::move(splitter.basic), std::move(splitter.preds));
}

void Verifier::apply_interference() {
	if (inside_atomic) return;
	auto [basic, extension] = split_annotation(std::move(current_annotation));
	
	// extract interference-free basic conjuncts
	auto current = std::make_unique<BasicConjunctionFormula>();
	bool changed;
	do {
		changed = false;
		for (auto& conjunct : basic->conjuncts) {
			if (!conjunct) continue;

			// add conjunct to current
			current->conjuncts.push_back(std::move(conjunct));
			conjunct.reset();

			// check if current is still interference free; if not, move conjunct back again
			if (!is_interference_free(*current)) {
				conjunct = std::move(current->conjuncts.back());
				current->conjuncts.pop_back();
			} else {
				changed = true;
			}
		}
	} while(changed);

	// keep interference-free part
	extension->conjuncts.insert(
		plankton::config.interference_insertion_at_begin ? extension->conjuncts.begin() : extension->conjuncts.end(),
		std::make_move_iterator(current->conjuncts.begin()),
		std::make_move_iterator(current->conjuncts.end())
	);
	current_annotation = std::move(extension);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool Verifier::is_interference_free(const BasicFormula& /*formula*/){
	// TODO: check Hoare Triple
	throw std::logic_error("not yet implemented (is_interference_free)");
}
