#include "plankton/backend/z3encoder.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


Z3Expr Z3Encoder::EncodeZ3(const BooleanValue& node, EncodingTag /*tag*/) {
	return MakeZ3Bool(node.value);
}

Z3Expr Z3Encoder::EncodeZ3(const NullValue& /*node*/, EncodingTag /*tag*/) {
	return MakeZ3NullPtr();
}

Z3Expr Z3Encoder::EncodeZ3(const EmptyValue& /*node*/, EncodingTag /*tag*/) {
	throw Z3EncodingError("Unsupported construct: instance of type 'EmptyValue' (aka 'EMPTY').");
}

Z3Expr Z3Encoder::EncodeZ3(const MaxValue& /*node*/, EncodingTag /*tag*/) {
	return MakeZ3MaxValue();
}

Z3Expr Z3Encoder::EncodeZ3(const MinValue& /*node*/, EncodingTag /*tag*/) {
	return MakeZ3MinValue();
}

Z3Expr Z3Encoder::EncodeZ3(const NDetValue& /*node*/, EncodingTag /*tag*/) {
	throw Z3EncodingError("Unsupported construct: instance of type 'NDetValue' (aka '*').");
}

Z3Expr Z3Encoder::EncodeZ3(const VariableExpression& node, EncodingTag tag) {
	return EncodeZ3(node.decl, tag);
}

Z3Expr Z3Encoder::EncodeZ3(const NegatedExpression& node, EncodingTag tag) {
	return EncodeZ3(*node.expr, tag).Neg();
}

Z3Expr Z3Encoder::EncodeZ3(const BinaryExpression& node, EncodingTag tag) {
	z3::expr lhs = EncodeZ3(*node.lhs, tag);
	z3::expr rhs = EncodeZ3(*node.rhs, tag);
	switch (node.op) {
		case BinaryExpression::Operator::EQ:  return (lhs == rhs);
		case BinaryExpression::Operator::NEQ: return (lhs != rhs);
		case BinaryExpression::Operator::LEQ: return (lhs <= rhs);
		case BinaryExpression::Operator::LT:  return (lhs < rhs);
		case BinaryExpression::Operator::GEQ: return (lhs >= rhs);
		case BinaryExpression::Operator::GT:  return (lhs > rhs);
		case BinaryExpression::Operator::AND: return (lhs && rhs);
		case BinaryExpression::Operator::OR:  return (lhs || rhs);
	}
}

Z3Expr Z3Encoder::EncodeZ3(const Dereference& node, EncodingTag tag) {
	auto expr = EncodeZ3(*node.expr, tag);
	Selector selector(node.expr->type(), node.fieldname);
	return EncodeHeap(expr, selector, tag);
}


Z3Expr Z3Encoder::EncodeZ3(const AxiomConjunctionFormula& formula, EncodingTag tag) {
	z3::expr_vector vec(context);
	for (const auto& conjunct : formula) {
		vec.push_back(EncodeZ3(*conjunct, tag));
	}
	return z3::mk_and(vec);
}

Z3Expr Z3Encoder::EncodeZ3(const ConjunctionFormula& formula, EncodingTag tag) {
	z3::expr_vector vec(context);
	for (const auto& conjunct : formula) {
		vec.push_back(EncodeZ3(*conjunct, tag));
	}
	return z3::mk_and(vec);
}

Z3Expr Z3Encoder::EncodeZ3(const NegatedAxiom& formula, EncodingTag tag) {
	return EncodeZ3(*formula.axiom, tag).Neg();
}

Z3Expr Z3Encoder::EncodeZ3(const ExpressionAxiom& formula, EncodingTag tag) {
	return EncodeZ3(*formula.expr, tag);
}

Z3Expr Z3Encoder::EncodeZ3(const ImplicationFormula& formula, EncodingTag tag) {
	auto premise = Encode(*formula.premise, tag);
	auto conclusion = Encode(*formula.conclusion, tag);
	return premise.Implies(conclusion);
}

Z3Expr Z3Encoder::EncodeZ3(const OwnershipAxiom& formula, EncodingTag tag) {
	return EncodeZ3IsOwned(EncodeZ3(*formula.expr, tag), tag);
}

Z3Expr Z3Encoder::EncodeZ3(const DataStructureLogicallyContainsAxiom& formula, EncodingTag tag) {
	auto node = EncodeZ3Variable(Sort::PTR, "qv-ptr", tag);
	auto key = EncodeZ3(*formula.value, tag);
	auto logicallyContains = EncodeZ3Predicate(*config.logicallyContainsKey, node, key, tag);
	auto keysetContains = EncodeZ3KeysetContains(node, key, tag);
	return z3::exists(node, keysetContains.And(logicallyContains));
}

Z3Expr Z3Encoder::EncodeZ3(const NodeLogicallyContainsAxiom& formula, EncodingTag tag) {
	auto node = EncodeZ3(*formula.node, tag);
	auto key = EncodeZ3(*formula.value, tag);
	return EncodeZ3Predicate(*config.logicallyContainsKey, node, key, tag);
}

Z3Expr Z3Encoder::EncodeZ3(const KeysetContainsAxiom& formula, EncodingTag tag) {
	auto node = EncodeZ3(*formula.node, tag);
	auto value = EncodeZ3(*formula.value, tag);
	return EncodeZ3KeysetContains(node, value, tag);
}

Z3Expr Z3Encoder::EncodeZ3(const HasFlowAxiom& formula, EncodingTag tag) {
	return EncodeZ3HasFlow(EncodeZ3(*formula.expr, tag), tag);
}

Z3Expr Z3Encoder::EncodeZ3(const FlowContainsAxiom& formula, EncodingTag tag) {
	auto node = EncodeZ3(*formula.expr, tag);
	auto key = EncodeZ3Variable(Sort::DATA, "qv-key", tag);
	z3::expr low = EncodeZ3(*formula.low_value, tag);
	z3::expr high = EncodeZ3(*formula.high_value, tag);
	auto keyInbetween = Z3Expr((low <= key) && (key <= high));
	auto flow = EncodeZ3Flow(node, key, tag);
	return z3::forall(key, keyInbetween.Implies(flow));
}

Z3Expr Z3Encoder::EncodeZ3(const ObligationAxiom& formula, EncodingTag tag) {
	return EncodeZ3Obligation(formula.kind, EncodeZ3(formula.key->decl, tag), tag);
}

Z3Expr Z3Encoder::EncodeZ3(const FulfillmentAxiom& formula, EncodingTag tag) {
	return EncodeZ3Fulfillment(formula.kind, EncodeZ3(formula.key->decl, tag), formula.return_value, tag);
}

Z3Expr Z3Encoder::EncodeZ3(const PastPredicate& /*formula*/, EncodingTag /*tag*/) {
	throw Z3EncodingError("Cannot encode 'PastPredicate'.");
}

Z3Expr Z3Encoder::EncodeZ3(const FuturePredicate& /*formula*/, EncodingTag /*tag*/) {
	throw Z3EncodingError("Cannot encode 'FuturePredicate'.");
}

Z3Expr Z3Encoder::EncodeZ3(const Annotation& formula, EncodingTag tag) {
	return EncodeZ3(*formula.now, tag);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct Z3EncoderCallback final : public cola::BaseVisitor, public BaseLogicVisitor {
	using cola::BaseVisitor::visit;
	using BaseLogicVisitor::visit;

	Z3Encoder& encoder;
	EncodingTag tag;
	std::optional<z3::expr> result;

	Z3EncoderCallback(Z3Encoder& encoder, EncodingTag tag) : encoder(encoder), tag(tag) {}
	z3::expr GetResult() { assert(result.has_value()); return result.value(); }
	z3::expr EncodeZ3(const cola::Expression& expression) { expression.accept(*this); return GetResult(); }
	z3::expr EncodeZ3(const Formula& formula) { formula.accept(*this); return GetResult(); }

	void visit(const VariableDeclaration& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const BooleanValue& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const NullValue& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const EmptyValue& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const MaxValue& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const MinValue& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const NDetValue& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const VariableExpression& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const NegatedExpression& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const BinaryExpression& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const Dereference& node) override { result = encoder.EncodeZ3(node, tag); }
	void visit(const AxiomConjunctionFormula& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const ConjunctionFormula& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const NegatedAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const ExpressionAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const ImplicationFormula& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const OwnershipAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const DataStructureLogicallyContainsAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const NodeLogicallyContainsAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const KeysetContainsAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const HasFlowAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const FlowContainsAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const ObligationAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const FulfillmentAxiom& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const PastPredicate& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const FuturePredicate& formula) override { result = encoder.EncodeZ3(formula, tag); }
	void visit(const Annotation& formula) override { result = encoder.EncodeZ3(formula, tag); }
};

Z3Expr Z3Encoder::EncodeZ3(const Formula& formula, EncodingTag tag) {
	return Z3EncoderCallback(*this, tag).EncodeZ3(formula);
}

Z3Expr Z3Encoder::EncodeZ3(const Expression& expression, EncodingTag tag) {
	return Z3EncoderCallback(*this, tag).EncodeZ3(expression);
}
