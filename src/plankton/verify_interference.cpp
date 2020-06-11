#include "plankton/verify.hpp"

#include <map>
#include <sstream>
#include "cola/util.hpp"
#include "cola/visitors.hpp"
#include "plankton/config.hpp"
#include "plankton/post.hpp"

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

	std::unique_ptr<VariableExpression> handle_variable(VariableExpression& expr) {
		return std::make_unique<VariableExpression>(info.rename(expr.decl));
	}

	void visit(VariableExpression& expr) override {
		assert(current_owner);
		assert(current_owner->get() == &expr);
		*current_owner = handle_variable(expr);
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

struct InterferenceFormulaRenamer : LogicNonConstVisitor {
	InterferneceExpressionRenamer expr_renamer;

	InterferenceFormulaRenamer(RenamingInfo& info_) : expr_renamer(info_) {}

	void visit(ConjunctionFormula& formula) override {
		for (auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}

	void visit(ExpressionFormula& formula) override {
		expr_renamer.handle_expression(formula.expr);
	}

	void visit(NegatedFormula& formula) override {
		formula.formula->accept(*this);
	}

	void visit(OwnershipFormula& formula) override {
		formula.expr = expr_renamer.handle_variable(*formula.expr);
	}

	void visit(LogicallyContainedFormula& formula) override {
		expr_renamer.handle_expression(formula.expr);
	}

	void visit(FlowFormula& formula) override {
		expr_renamer.handle_expression(formula.expr);
	}

	void visit(PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const PastPredicate&)"); }
	void visit(FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const FuturePredicate&)"); }
	void visit(Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const Annotation&)"); }
};

void Verifier::extend_interference(const cola::Assignment& command) {
	// make effect: (rename(current_annotation.now), rename(command)) where rename(x) renames the local variables in x
	InterferenceFormulaRenamer renamer(interference_renaming_info);
	auto pre = plankton::copy(*current_annotation->now);
	pre->accept(renamer);
	auto cmd = std::make_unique<Assignment>(cola::copy(*command.lhs), cola::copy(*command.rhs));
	renamer.expr_renamer.handle_expression(cmd->lhs);
	renamer.expr_renamer.handle_expression(cmd->rhs);
	auto effect = std::make_unique<Effect>(std::move(pre), std::move(cmd));

	// extend interference
	extend_interference(std::move(effect));
}

bool commands_equal(const Command& cmd, const Command& other) {
	static std::map<std::pair<std::size_t, std::size_t>, bool> lookup;

	std::pair<std::size_t, std::size_t> key;
	key = cmd.id <= other.id ? std::make_pair(cmd.id, other.id) : std::make_pair(other.id, cmd.id);
	auto find = lookup.find(key);
	if (find != lookup.end()) {
		return find->second;
	} else {

		// TODO: find a better way to check for equivalence
		std::stringstream cmdStream, otherStream;
		cola::print(cmd, cmdStream);
		cola::print(other, otherStream);
		bool result = cmdStream.str() == otherStream.str();
		lookup[key] = result;
		return result;
	}
}

void Verifier::extend_interference(std::unique_ptr<Effect> effect) {
	bool is_effect_new = true;
	bool effect_pruned = false;

	// check if we already covered the new effect; delete effects covered by the new one
	for (auto& other : interference) {
		if (!commands_equal(*effect->command, *other->command)) {
			continue;
		}

		if (plankton::implies(*effect->precondition, *other->precondition)) {
			is_effect_new = false;
			break;
		}
		if (plankton::implies(*other->precondition, *effect->precondition)) {
			other.reset();
			effect_pruned = true;
		}
	}

	// remove pruned effects
	if (effect_pruned) {
		std::deque<std::unique_ptr<Effect>> new_interference;
		for (auto& other : interference) {
			if (other) {
				new_interference.push_back(std::move(other));
			}
		}
		interference = std::move(new_interference);
	}

	// add new effect if needed
	if (is_effect_new) {
		interference.push_back(std::move(effect));
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::apply_interference() {
	if (inside_atomic) return;
	auto& now = *current_annotation->now; // TODO: ensure that current_annotation->now does not contain nested conjunctions
	
	// find strongest formula that is interference
	auto stable = std::make_unique<ConjunctionFormula>();
	bool changed;
	do {
		changed = false;
		for (auto& conjunct : now.conjuncts) {
			if (!conjunct) continue;

			// add conjunct to stable
			stable->conjuncts.push_back(std::move(conjunct));
			conjunct.reset();

			// check if stable is still interference free; if not, move conjunct back again
			if (!is_interference_free(*stable)) {
				conjunct = std::move(stable->conjuncts.back());
				stable->conjuncts.pop_back();
			} else {
				changed = true;
			}
		}
	} while(changed);

	// keep interference-free part
	current_annotation->now = std::move(stable);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

inline std::unique_ptr<Annotation> merge_for_interference(const ConjunctionFormula& formula, const ConjunctionFormula& other) {
	auto annotation = std::make_unique<Annotation>(plankton::copy(formula));
	auto remaining = plankton::copy(other);
	annotation->now->conjuncts.insert(
		annotation->now->conjuncts.end(),
		std::make_move_iterator(remaining->conjuncts.begin()),
		std::make_move_iterator(remaining->conjuncts.end())
	);
	return annotation;
}

bool Verifier::is_interference_free(const ConjunctionFormula& formula){
	for (const auto& effect : interference) {
		auto pre = merge_for_interference(formula, *effect->precondition);

		// TODO: have a light-weight check first?
		auto post = plankton::post(std::move(pre), *effect->command);
		if (!plankton::implies(*post->now, formula)) {
			return false;
		}
	}

	return true;
}
