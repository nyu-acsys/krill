#include "../../../include/prover/verify.hpp"

using namespace cola;
using namespace heal;
using namespace prover;


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
	void enter(const DataStructureLogicallyContainsAxiom& /*formula*/) override { result = true; }
	void enter(const NodeLogicallyContainsAxiom& /*formula*/) override { result = true; }
	void enter(const KeysetContainsAxiom& /*formula*/) override { result = true; }
	void enter(const HasFlowAxiom& /*formula*/) override { result = true; }
	void enter(const FlowContainsAxiom& /*formula*/) override { result = true; }
	void enter(const UniqueInflowAxiom& /*formula*/) override { result = true; }
};

bool Verifier::has_effect(const SimpleFormula& formula) {
	LogicEffectSearcher searcher;
	formula.accept(searcher);
	return searcher.result;
}
