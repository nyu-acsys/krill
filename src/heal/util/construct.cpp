#include "heal/util.hpp"

using namespace cola;
using namespace heal;

std::unique_ptr<NullValue> heal::MakeNullExpr() {
	return std::make_unique<NullValue>();
}

std::unique_ptr<BooleanValue> heal::MakeBoolExpr(bool val) {
	return std::make_unique<BooleanValue>(val);
}

std::unique_ptr<BooleanValue> heal::MakeTrueExpr() {
	return MakeBoolExpr(true);
}

std::unique_ptr<BooleanValue> heal::MakeFalseExpr() {
	return MakeBoolExpr(false);
}

std::unique_ptr<MaxValue> heal::MakeMaxExpr() {
	return std::make_unique<MaxValue>();
}

std::unique_ptr<MinValue> heal::MakeMinExpr() {
	return std::make_unique<MinValue>();
}


std::unique_ptr<VariableExpression> heal::MakeExpr(const VariableDeclaration& decl) {
	return std::make_unique<VariableExpression>(decl);
}

std::unique_ptr<BinaryExpression> heal::MakeExpr(BinaryExpression::Operator op, std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs) {
	return std::make_unique<BinaryExpression>(op, std::move(lhs), std::move(rhs));
}

std::unique_ptr<Dereference> heal::MakeDerefExpr(const VariableDeclaration& decl, std::string field) {
	return std::make_unique<Dereference>(MakeExpr(decl), std::move(field));
}


std::unique_ptr<ExpressionAxiom> heal::MakeAxiom(std::unique_ptr<Expression> expr) {
	return std::make_unique<ExpressionAxiom>(std::move(expr));
}

std::unique_ptr<OwnershipAxiom> heal::MakeOwnershipAxiom(const VariableDeclaration& decl) {
	return std::make_unique<OwnershipAxiom>(MakeExpr(decl));
}

std::unique_ptr<NegationFormula> heal::MakeNegation(std::unique_ptr<Formula> axiom) {
	return std::make_unique<NegationFormula>(std::move(axiom));
}

std::unique_ptr<ObligationAxiom> heal::MakeObligationAxiom(SpecificationAxiom::Kind kind, const VariableDeclaration& decl) {
	return std::make_unique<ObligationAxiom>(kind, MakeExpr(decl));
}

std::unique_ptr<FulfillmentAxiom> heal::MakeFulfillmentAxiom(SpecificationAxiom::Kind kind, const VariableDeclaration& decl, bool returnValue) {
	return std::make_unique<FulfillmentAxiom>(kind, MakeExpr(decl), returnValue);
}

std::unique_ptr<DataStructureLogicallyContainsAxiom> heal::MakeDatastructureContainsAxiom(std::unique_ptr<Expression> value) {
	return std::make_unique<DataStructureLogicallyContainsAxiom>(std::move(value));
}

std::unique_ptr<NodeLogicallyContainsAxiom> heal::MakeNodeContainsAxiom(std::unique_ptr<Expression> node, std::unique_ptr<Expression> value) {
	return std::make_unique<NodeLogicallyContainsAxiom>(std::move(node), std::move(value));
}

std::unique_ptr<HasFlowAxiom> heal::MakeHasFlowAxiom(std::unique_ptr<Expression> expr) {
	return std::make_unique<HasFlowAxiom>(std::move(expr));
}

std::unique_ptr<KeysetContainsAxiom> heal::MakeKeysetContainsAxiom(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	return std::make_unique<KeysetContainsAxiom>(std::move(expr), std::move(other));
}

std::unique_ptr<FlowContainsAxiom> heal::MakeFlowContainsAxiom(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> low, std::unique_ptr<Expression> high) {
	return std::make_unique<FlowContainsAxiom>(std::move(expr), std::move(low), std::move(high));
}

std::unique_ptr<UniqueInflowAxiom> heal::MakeUniqueInflowAxiom(std::unique_ptr<Expression> expr) {
	return std::make_unique<UniqueInflowAxiom>(std::move(expr));
}


std::unique_ptr<ConjunctionFormula> heal::MakeConjunction() {
	return std::make_unique<ConjunctionFormula>();
}

std::unique_ptr<ImplicationFormula> heal::MakeImplication() {
	return std::make_unique<ImplicationFormula>();
}

std::unique_ptr<ImplicationFormula> heal::MakeImplication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion) {
	auto result = std::make_unique<ImplicationFormula>();
	result->premise = std::move(premise);
	result->premise = std::move(conclusion);
	return result;
}
