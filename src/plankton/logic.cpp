#include "plankton/logic.hpp"

#include <cassert>

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
