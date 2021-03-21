#include "heal/util.hpp"

using namespace cola;
using namespace heal;

std::unique_ptr<SymbolicNull> heal::MakeNull() {
	return std::make_unique<SymbolicNull>();
}

std::unique_ptr<SymbolicBool> heal::MakeBool(bool val) {
	return std::make_unique<SymbolicBool>(val);
}

std::unique_ptr<SymbolicBool> heal::MakeTrue() {
	return heal::MakeBool(true);
}

std::unique_ptr<SymbolicBool> heal::MakeFalse() {
	return heal::MakeBool(false);
}

std::unique_ptr<SymbolicMax> heal::MakeMax() {
	return std::make_unique<SymbolicMax>();
}

std::unique_ptr<SymbolicMin> heal::MakeMin() {
	return std::make_unique<SymbolicMin>();
}


std::unique_ptr<LogicVariable> heal::MakeVar(const VariableDeclaration& decl) {
	return std::make_unique<LogicVariable>(decl);
}

std::unique_ptr<NegatedAxiom> heal::MakeNegation(std::unique_ptr<Axiom> axiom) {
    return std::make_unique<NegatedAxiom>(std::move(axiom));
}

std::unique_ptr<BoolAxiom> heal::MakeBoolAxiom(bool value) {
    return std::make_unique<BoolAxiom>(value);
}

std::unique_ptr<PointsToAxiom> heal::MakePointsToAxiom(const VariableDeclaration& node, std::string field, const VariableDeclaration& value) {
    return std::make_unique<PointsToAxiom>(heal::MakeVar(node), std::move(field), heal::MakeVar(value));
}

std::unique_ptr<StackAxiom> heal::MakeStackAxiom(std::unique_ptr<StackExpression> lhs, StackAxiom::Operator op, std::unique_ptr<StackExpression> rhs) {
    return std::make_unique<StackAxiom>(std::move(lhs), op, std::move(rhs));
}

std::unique_ptr<OwnershipAxiom> heal::MakeOwnershipAxiom(const VariableDeclaration& decl) {
	return std::make_unique<OwnershipAxiom>(heal::MakeVar(decl));
}

std::unique_ptr<ObligationAxiom> heal::MakeObligationAxiom(SpecificationAxiom::Kind kind, const VariableDeclaration& decl) {
	return std::make_unique<ObligationAxiom>(kind, MakeVar(decl));
}

std::unique_ptr<FulfillmentAxiom> heal::MakeFulfillmentAxiom(SpecificationAxiom::Kind kind, const VariableDeclaration& decl, bool returnValue) {
	return std::make_unique<FulfillmentAxiom>(kind, MakeVar(decl), returnValue);
}

std::unique_ptr<DataStructureLogicallyContainsAxiom> heal::MakeDatastructureContainsAxiom(const VariableDeclaration& value) {
	return std::make_unique<DataStructureLogicallyContainsAxiom>(heal::MakeVar(value));
}

std::unique_ptr<NodeLogicallyContainsAxiom> heal::MakeNodeContainsAxiom(const VariableDeclaration& node, const VariableDeclaration& value) {
	return std::make_unique<NodeLogicallyContainsAxiom>(heal::MakeVar(node), heal::MakeVar(value));
}

std::unique_ptr<HasFlowAxiom> heal::MakeHasFlowAxiom(const VariableDeclaration& expr) {
	return std::make_unique<HasFlowAxiom>(heal::MakeVar(expr));
}

std::unique_ptr<KeysetContainsAxiom> heal::MakeKeysetContainsAxiom(const VariableDeclaration& expr, const VariableDeclaration& other) {
	return std::make_unique<KeysetContainsAxiom>(heal::MakeVar(expr), heal::MakeVar(other));
}

std::unique_ptr<FlowContainsAxiom> heal::MakeFlowContainsAxiom(const VariableDeclaration& expr, std::unique_ptr<StackExpression> low, std::unique_ptr<StackExpression> high) {
	return std::make_unique<FlowContainsAxiom>(heal::MakeVar(expr), std::move(low), std::move(high));
}

std::unique_ptr<UniqueInflowAxiom> heal::MakeUniqueInflowAxiom(const VariableDeclaration& expr) {
	return std::make_unique<UniqueInflowAxiom>(heal::MakeVar(expr));
}

std::unique_ptr<StackDisjunction> heal::MakeDisjunction(std::deque<std::unique_ptr<StackAxiom>> disjuncts) {
    return std::make_unique<StackDisjunction>(std::move(disjuncts));
}

std::unique_ptr<SeparatingConjunction> heal::MakeConjunction(std::deque<std::unique_ptr<Formula>> conjuncts) {
    return std::make_unique<SeparatingConjunction>(std::move(conjuncts));
}

std::unique_ptr<FlatSeparatingConjunction> heal::MakeFlatConjunction(std::deque<std::unique_ptr<FlatFormula>> conjuncts) {
    auto result = std::make_unique<FlatSeparatingConjunction>();
    auto Add = [&result](FlatFormula* take) {
        if (auto axiom = dynamic_cast<Axiom*>(take)) {
            result->conjuncts.emplace_back(axiom);
        } else if (auto conjunction = dynamic_cast<FlatSeparatingConjunction*>(take)) {
            std::move(conjunction->conjuncts.begin(), conjunction->conjuncts.end(), std::back_inserter(result->conjuncts));
        } else {
            throw std::logic_error("Construction error: unexpected object in AST.");
        }
    };
    for (auto& conjunct : conjuncts) {
        Add(conjunct.release());
    }
    return result;
}

std::unique_ptr<SeparatingImplication> heal::MakeImplication() {
    return std::make_unique<SeparatingImplication>(MakeBoolAxiom(false), MakeBoolAxiom(true));
}
std::unique_ptr<SeparatingImplication> heal::MakeImplication(std::unique_ptr<FlatSeparatingConjunction> premise, std::unique_ptr<FlatSeparatingConjunction> conclusion) {
    return std::make_unique<SeparatingImplication>(std::move(premise), std::move(conclusion));
}
