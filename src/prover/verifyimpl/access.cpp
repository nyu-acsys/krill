#include "../../../include/prover/verify.hpp"

#include <set>

using namespace cola;
using namespace heal;
using namespace prover;


struct DereferencedExpressionCollector : BaseVisitor {
	std::set<const Expression*> exprs;

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }

	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }

	void visit(const Dereference& node) override {
		exprs.insert(node.expr.get());
		node.expr->accept(*this);
	}

	void visit(const Skip& /*node*/) override { /* do nothing */ }
	void visit(const Break& /*node*/) override { /* do nothing */ }
	void visit(const Continue& /*node*/) override { /* do nothing */ }
	void visit(const Malloc& /*node*/) override { /* do nothing */ }
	void visit(const Assume& node) override { node.expr->accept(*this); }
	void visit(const Assert& node) override { node.expr->accept(*this); }
	void visit(const Return& node) override {
		for (const auto& expr : node.expressions) {
			expr->accept(*this);
		}
	}
	void visit(const Assignment& node) override { node.lhs->accept(*this); node.rhs->accept(*this);}
	void visit(const CompareAndSwap& node) override {
		for (const auto& elem : node.elems) {
			elem.dst->accept(*this);
			elem.cmp->accept(*this);
			elem.src->accept(*this);
		}
	}
};

void Verifier::check_pointer_accesses(const Command& command) {
	DereferencedExpressionCollector collector;
	command.accept(collector);

	for (const Expression* expr : collector.exprs) {
		if (!solver->ImpliesIsNonNull(*current_annotation, *expr)) {
			throw UnsafeDereferenceError(*expr, command);
		}
	}
}
