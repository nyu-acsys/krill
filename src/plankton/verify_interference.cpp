#include "plankton/verify.hpp"

#include <iostream> // TODO: delete
#include <map>
#include "cola/util.hpp"
#include "cola/visitors.hpp"
#include "plankton/config.hpp"

using namespace cola;
using namespace plankton;


struct EffectSearcher : public BaseVisitor {
	bool result = false;

	static bool has_effect(const Expression& expr) {
		EffectSearcher searcher;
		expr.accept(searcher);
		return searcher.result;
	}

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }

	bool is_owned(const Expression& /*expr*/) {
		// TODO: return !("node.expr is var" && is_owned(node.expr))
		return false;
	}

	void visit(const VariableExpression& node) override { result |= node.decl.is_shared; }
	void visit(const Dereference& node) override { result |= !is_owned(*node.expr); }
	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
};

bool Verifier::has_effect(const Expression& assignee) {
	return EffectSearcher::has_effect(assignee);
}

struct LogicEffectSearcher : public DefaultListener {
	bool result = false;

	void enter(const ExpressionAxiom& formula) override { result |= EffectSearcher::has_effect(*formula.expr); }
	void enter(const LogicallyContainedAxiom& /*formula*/) override { result = true; }
	void enter(const KeysetContainsAxiom& /*formula*/) override { result = true; }
	void enter(const HasFlowAxiom& /*formula*/) override { result = true; }
	void enter(const FlowContainsAxiom& /*formula*/) override { result = true; }
};

bool Verifier::has_effect(const SimpleFormula& formula) {
	LogicEffectSearcher searcher;
	formula.accept(searcher);
	return searcher.result;
}


bool commands_equal(const Assignment& cmd, const Assignment& other) {
	static std::map<std::pair<std::size_t, std::size_t>, bool> lookup;

	std::pair<std::size_t, std::size_t> key;
	key = cmd.id <= other.id ? std::make_pair(cmd.id, other.id) : std::make_pair(other.id, cmd.id);

	auto find = lookup.find(key);
	if (find != lookup.end()) {
		return find->second;

	} else {
		bool result = plankton::syntactically_equal(*cmd.lhs, *other.lhs) && plankton::syntactically_equal(*cmd.rhs, *other.rhs);
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

		// TODO: implement iterative solving => extend effects with an ImplicationChecker?
		if (solver->Implies(*effect->precondition, *other->precondition)) {
			is_effect_new = false;
			break;
		}
		if (solver->Implies(*other->precondition, *effect->precondition)) {
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

void Verifier::extend_interference(const cola::Assignment& command) {
	current_annotation = solver->AddInvariant(std::move(current_annotation));
	auto transformer = interference_renaming_info.as_transformer();

	// make effect: (rename(current_annotation.now), rename(command)) where rename(x) renames the local variables in x
	auto pre = plankton::replace_expression(plankton::copy(*current_annotation->now), transformer);
	auto cmd = std::make_unique<Assignment>(
		plankton::replace_expression(cola::copy(*command.lhs), transformer),
		plankton::replace_expression(cola::copy(*command.rhs), transformer)
	);
	auto effect = std::make_unique<Effect>(std::move(pre), std::move(cmd));

	// extend interference
	extend_interference(std::move(effect));
}


void Verifier::apply_interference() {
	if (inside_atomic) return;
	auto stable = std::make_unique<ConjunctionFormula>();
	current_annotation = solver->StripInvariant(std::move(current_annotation));

	std::cout << std::endl << "∆∆∆ applying interference " << current_annotation->now->conjuncts.size() << std::endl;
	std::size_t counter = 0;

	// quick check
	for (auto& conjunct : current_annotation->now->conjuncts) {
		if (!has_effect(*conjunct)) {
			stable->conjuncts.push_back(std::move(conjunct));
			conjunct.reset();
		}
	}

	// deep check
	bool changed;
	do {
		changed = false;
		for (auto& conjunct : current_annotation->now->conjuncts) {
			if (!conjunct) continue;
			++counter;

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
	} while(changed && plankton::config.interference_exhaustive_repetition);

	// keep interference-free part
	current_annotation->now = std::move(stable);

	std::cout << "    => number of interference freedom checks: " << counter << std::endl;
}


bool Verifier::is_interference_free(const ConjunctionFormula& formula){
	for (const auto& effect : interference) {
		if (!solver->PostEntails(*effect->precondition, *effect->command, formula)) {
			// std::cout << " ==> no" << std::endl;
			return false;
		}
	}
	return true;
}
