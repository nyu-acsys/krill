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

	void visit(VariableExpression& expr) override {
		assert(current_owner);
		assert(current_owner->get() == &expr);
		*current_owner = std::make_unique<VariableExpression>(info.rename(expr.decl));
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

struct InterferneceCommandRenamer : public BaseVisitor {
	RenamingInfo& info;
	InterferneceExpressionRenamer expr_renamer;
	std::unique_ptr<Command> result;

	InterferneceCommandRenamer(RenamingInfo& info_) : info(info_), expr_renamer(info_) {}

	void visit(const Assume& node) override {
		auto expr = cola::copy(*node.expr);
		expr_renamer.handle_expression(expr);
		result = std::make_unique<Assume>(std::move(expr));
	};

	void visit(const Assert& node) override {
		auto expr = cola::copy(*node.expr);
		expr_renamer.handle_expression(expr);
		result = std::make_unique<Assert>(std::move(expr));
	};

	void visit(const Malloc& node) override {
		result = std::make_unique<Malloc>(info.rename(node.lhs));
	};

	void visit(const Assignment& node) override {
		auto lhs = cola::copy(*node.lhs);
		expr_renamer.handle_expression(lhs);
		auto rhs = cola::copy(*node.rhs);
		expr_renamer.handle_expression(rhs);
		result = std::make_unique<Assignment>(std::move(lhs), std::move(rhs));
	};

	void visit(const Macro& node) override {
		auto copy = std::make_unique<Macro>(node.decl);
		for (const auto& expr : node.args) {
			auto arg = cola::copy(*expr);
			expr_renamer.handle_expression(arg);
			copy->args.push_back(std::move(arg));
		}
		for (const auto& decl : node.lhs) {
			copy->lhs.push_back(info.rename(decl));
		}
		result = std::move(copy);
	};

	void visit(const CompareAndSwap& node) override {
		auto copy = std::make_unique<CompareAndSwap>();
		for (const auto& elem : node.elems) {
			auto dst = cola::copy(*elem.dst);
			auto cmp = cola::copy(*elem.cmp);
			auto src = cola::copy(*elem.src);
			expr_renamer.handle_expression(dst);
			expr_renamer.handle_expression(cmp);
			expr_renamer.handle_expression(src);
			copy->elems.emplace_back(std::move(dst), std::move(cmp), std::move(src));
		}
		result = std::move(copy);
	};

	void visit(const Return& node) override {
		auto copy = std::make_unique<Return>();
		for (const auto& expr : node.expressions) {
			auto renamed = cola::copy(*expr);
			expr_renamer.handle_expression(renamed);
			copy->expressions.push_back(std::move(renamed));
		}
		result = std::move(copy);
	};

	void visit(const Skip& node) override {
		result = cola::copy(node);
	};

	void visit(const Break& node) override {
		result = cola::copy(node);
	};

	void visit(const Continue& node) override {
		result = cola::copy(node);
	};
};

struct InterferenceFormulaRenamer : LogicVisitor {
	RenamingInfo& info;
	InterferneceExpressionRenamer expr_renamer;
	std::unique_ptr<Formula> result;

	InterferenceFormulaRenamer(RenamingInfo& info_) : info(info_), expr_renamer(info_) {}

	void visit(const ConjunctionFormula& formula) override {
		auto copy = std::make_unique<ConjunctionFormula>();
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
			copy->conjuncts.push_back(std::move(result));
		}
		result = std::move(copy);
	}

	void visit(const ExpressionFormula& formula) override {
		auto expr = cola::copy(*formula.expr);
		expr_renamer.handle_expression(expr);
		result = std::make_unique<ExpressionFormula>(std::move(expr));
	}

	void visit(const PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const PastPredicate&)"); }
	void visit(const FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const FuturePredicate&)"); }
	void visit(const Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const Annotation&)"); }
};

void Verifier::extend_interference(const cola::Command& command) {
	// make effect: (rename(current_annotation.now), rename(command)) where rename(x) renames the local variables in x
	InterferneceCommandRenamer crenamer(interference_renaming_info);
	command.accept(crenamer);
	InterferenceFormulaRenamer frenamer(interference_renaming_info);
	current_annotation->now->accept(frenamer);
	auto effect = std::make_unique<Effect>(std::move(frenamer.result), std::move(crenamer.result));

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

	throw std::logic_error("not yet implemented");
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

bool Verifier::is_interference_free(const Formula& formula){
	for (const auto& effect : interference) {
		auto pre = std::make_unique<Annotation>();
		pre->now->conjuncts.push_back(plankton::copy(formula));
		pre->now->conjuncts.push_back(plankton::copy(*effect->precondition));
		auto cmd = dynamic_cast<const Command*>()

		// TODO: have a light-weight check first?
		auto post = plankton::post(std::move(pre), *effect->command);
		if (!plankton::implies(*post->now, formula)) {
			return false;
		}
	}

	return true;
}
