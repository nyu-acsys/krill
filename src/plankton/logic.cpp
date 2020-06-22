#include "plankton/logic.hpp"

#include <cassert>
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


ImplicationFormula::ImplicationFormula() : premise(std::make_unique<AxiomConjunctionFormula>()), conclusion(std::make_unique<AxiomConjunctionFormula>()) {
}

ImplicationFormula::ImplicationFormula(std::unique_ptr<AxiomConjunctionFormula> premise_, std::unique_ptr<AxiomConjunctionFormula> conclusion_)
 : premise(std::move(premise_)), conclusion(std::move(conclusion_)) {
	assert(premise);
	assert(conclusion);
}

ImplicationFormula::ImplicationFormula(std::unique_ptr<Axiom> premise_, std::unique_ptr<Axiom> conclusion_) : ImplicationFormula() {
	assert(premise_);
	assert(conclusion_);
	premise->conjuncts.push_back(std::move(premise_));
	conclusion->conjuncts.push_back(std::move(conclusion_));
}

NegatedAxiom::NegatedAxiom(std::unique_ptr<Axiom> axiom_) : axiom(std::move(axiom_)) {
	assert(axiom);
}

ExpressionAxiom::ExpressionAxiom(std::unique_ptr<cola::Expression> expr_) : expr(std::move(expr_)) {
	assert(expr);
	assert(expr->type() == cola::Type::bool_type());
}

OwnershipAxiom::OwnershipAxiom(std::unique_ptr<VariableExpression> expr_) : expr(std::move(expr_)) {
	assert(expr);
	assert(!expr->decl.is_shared);
	assert(expr->decl.type.sort == Sort::PTR);
}

LogicallyContainedAxiom::LogicallyContainedAxiom(std::unique_ptr<Expression> expr_) : expr(std::move(expr_)) {
	assert(expr);
	assert(expr->sort() == Sort::DATA);
}

KeysetContainsAxiom::KeysetContainsAxiom(std::unique_ptr<Expression> node_, std::unique_ptr<Expression> value_)
 : node(std::move(node_)), value(std::move(value_)) {
	assert(node);
	assert(node->sort() == Sort::PTR);
	assert(value);
	assert(value->sort() == Sort::DATA);
}

FlowAxiom::FlowAxiom(std::unique_ptr<Expression> expr_, FlowValue flow_) : expr(std::move(expr_)), flow(flow_) {
	assert(expr);
	assert(expr->sort() == Sort::PTR);
}

SpecificationAxiom::SpecificationAxiom(Kind kind_, std::unique_ptr<cola::VariableExpression> key_) : kind(kind_), key(std::move(key_)) {
	assert(key);
	assert(key->sort() == Sort::DATA);
}

ObligationAxiom::ObligationAxiom(Kind kind_, std::unique_ptr<VariableExpression> key_) : SpecificationAxiom(kind_, std::move(key_)) {
}

FulfillmentAxiom::FulfillmentAxiom(Kind kind_, std::unique_ptr<VariableExpression> key_, bool return_value_)
 : SpecificationAxiom(kind_, std::move(key_)), return_value(return_value_) {
}

PastPredicate::PastPredicate(std::unique_ptr<ConjunctionFormula> formula_) : formula(std::move(formula_)) {
	assert(formula);
}

FuturePredicate::FuturePredicate(std::unique_ptr<ConjunctionFormula> pre_, std::unique_ptr<Assignment> cmd_, std::unique_ptr<ConjunctionFormula> post_)
 : pre(std::move(pre_)), command(std::move(cmd_)), post(std::move(post_)) {
	assert(pre);
	assert(command);
	assert(post);
}

Annotation::Annotation() : now(std::make_unique<ConjunctionFormula>()) {
}

Annotation::Annotation(std::unique_ptr<ConjunctionFormula> now_) : now(std::move(now_)) {
	assert(now);
}

inline std::unique_ptr<Annotation> mk_bool(bool value) {
	auto result = std::make_unique<Annotation>();
	result->now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BooleanValue>(value)));
	return result;
}

std::unique_ptr<Annotation> Annotation::make_true() {
	return mk_bool(true);
}

std::unique_ptr<Annotation> Annotation::make_false() {
	return mk_bool(false);
}


struct DereferenceChecker : BaseVisitor {
	bool result = true;
	bool inside_deref = false;

	static bool is_node_local(const Expression& expr) {
		DereferenceChecker visitor;
		expr.accept(visitor);
		return visitor.result;
	}

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }

	void visit(const NegatedExpression& node) override {
		if (result) node.expr->accept(*this);
	}
	void visit(const BinaryExpression& node) override {
		if (result) node.lhs->accept(*this);
		if (result) node.rhs->accept(*this);
	}
	void visit(const Dereference& node) override {
		if (inside_deref) result = false;
		if (!result) return;
		inside_deref = true;
		node.expr->accept(*this);
		inside_deref = false;
	}
};

NodeInvariant::NodeInvariant(const cola::Program& source_) : source(source_) {
	for (const auto& inv : source.invariants) {
		if (inv->vars.size() != 1 || inv->vars.at(0)->type.sort != Sort::PTR) {
			throw std::logic_error("Cannot construct node invariant from given program, invariant '" + inv->name + "' is malformed.");
		}
		if (!DereferenceChecker::is_node_local(*inv->expr)) {
			throw std::logic_error("Cannot construct node invariant from given program, invariant '" + inv->name + "' is not node-local.");
		}
	}
}

std::unique_ptr<ConjunctionFormula> NodeInvariant::instantiate(const VariableDeclaration& var) const {
	// TODO: should we really store them? With instantiations for dummy/temp variables this map might grow unnecessary large
	static std::map<const VariableDeclaration*, std::unique_ptr<ConjunctionFormula>> var2res;

	auto find = var2res.find(&var);
	if (find != var2res.end()) {
		return plankton::copy(*find->second);
	}

	auto result = std::make_unique<ConjunctionFormula>();
	for (const auto& property : source.invariants) {
		result->conjuncts.push_back(plankton::instantiate_property(*property, { var }));
	}
	var2res[&var] = plankton::copy(*result);
	return result;
}
