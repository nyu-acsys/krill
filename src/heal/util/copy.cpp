#include "heal/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace heal;

template<typename T>
std::deque<std::unique_ptr<T>> CopyAll(const std::deque<std::unique_ptr<T>>& elems) {
    std::deque<std::unique_ptr<T>> result;
    for (const auto& elem : elems) {
        result.push_back(heal::Copy(*elem));
    }
    return result;
}

std::unique_ptr<SymbolicVariable> heal::Copy(const SymbolicVariable& expression) {
    return std::make_unique<SymbolicVariable>(expression.Decl());
}

std::unique_ptr<SeparatingConjunction> heal::Copy(const SeparatingConjunction& formula) {
    return std::make_unique<SeparatingConjunction>(CopyAll(formula.conjuncts));
}

std::unique_ptr<SymbolicAxiom> heal::Copy(const SymbolicAxiom& formula) {
    return std::make_unique<SymbolicAxiom>(heal::Copy(*formula.lhs), formula.op, heal::Copy(*formula.rhs));
}

std::unique_ptr<ObligationAxiom> heal::Copy(const ObligationAxiom& formula) {
    return std::make_unique<ObligationAxiom>(formula.kind, heal::Copy(*formula.key));
}

std::unique_ptr<Annotation> heal::Copy(const Annotation& annotation) {
    return std::make_unique<Annotation>(heal::Copy(*annotation.now), CopyAll(annotation.time));
}

struct StackExpressionReplicator : public BaseLogicVisitor {
    std::unique_ptr<SymbolicExpression> result;

    void visit(const SymbolicVariable& expression) override { result = heal::Copy(expression); }
    void visit(const SymbolicBool& expression) override { result = std::make_unique<SymbolicBool>(expression.value); }
    void visit(const SymbolicNull& /*expression*/) override { result = std::make_unique<SymbolicNull>(); }
    void visit(const SymbolicMin& /*expression*/) override { result = std::make_unique<SymbolicMin>(); }
    void visit(const SymbolicMax& /*expression*/) override { result = std::make_unique<SymbolicMax>(); }

};

std::unique_ptr<SymbolicExpression> heal::Copy(const SymbolicExpression& expression) {
    static StackExpressionReplicator replicator;
    expression.accept(replicator);
    return std::move(replicator.result);
}

struct AxiomReplicator : public BaseLogicVisitor {
    std::unique_ptr<Axiom> result;

    void visit(const SymbolicAxiom& formula) override {
        result = heal::Copy(formula);
    }
    void visit(const PointsToAxiom& formula) override {
        decltype(PointsToAxiom::fieldToValue) fieldToValue;
        for (const auto& [field, value] : formula.fieldToValue) {
            fieldToValue[field] = heal::Copy(*value);
        }
        result = std::make_unique<PointsToAxiom>(heal::Copy(*formula.node), formula.isLocal, formula.flow, std::move(fieldToValue));
    }
    void visit(const EqualsToAxiom& formula) override {
        result = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(formula.variable->decl), heal::Copy(*formula.value));
    }
    void visit(const StackDisjunction& formula) override {
        result = std::make_unique<StackDisjunction>(CopyAll(formula.axioms));
    }
    void visit(const InflowContainsValueAxiom& formula) override {
        result = std::make_unique<InflowContainsValueAxiom>(formula.flow, heal::Copy(*formula.value));
    }
    void visit(const InflowContainsRangeAxiom& formula) override {
        result = std::make_unique<InflowContainsRangeAxiom>(formula.flow, heal::Copy(*formula.valueLow), heal::Copy(*formula.valueHigh));
    }
    void visit(const InflowEmptinessAxiom& formula) override {
        result = std::make_unique<InflowEmptinessAxiom>(formula.flow, formula.isEmpty);
    }
    void visit(const ObligationAxiom& formula) override {
        result = heal::Copy(formula);
    }
    void visit(const FulfillmentAxiom& formula) override {
        result = std::make_unique<FulfillmentAxiom>(formula.kind, heal::Copy(*formula.key), formula.return_value);
    }

};

std::unique_ptr<Axiom> heal::Copy(const Axiom& formula) {
    static AxiomReplicator replicator;
    formula.accept(replicator);
    return std::move(replicator.result);
}

struct FormulaReplicator : public BaseLogicVisitor {
    std::unique_ptr<Formula> result;

    void visit(const SeparatingImplication& formula) override {
        result = std::make_unique<SeparatingImplication>(heal::Copy(*formula.premise), heal::Copy(*formula.conclusion));
    }
    void visit(const SeparatingConjunction& formula) override { result = heal::Copy(formula); }
    void visit(const PointsToAxiom& axiom) override { result = heal::Copy(axiom); }
    void visit(const EqualsToAxiom& axiom) override { result = heal::Copy(axiom); }
    void visit(const SymbolicAxiom& axiom) override { result = heal::Copy(axiom); }
    void visit(const StackDisjunction& axiom) override { result = heal::Copy(axiom); }
    void visit(const InflowContainsValueAxiom& axiom) override { result = heal::Copy(axiom); }
    void visit(const InflowContainsRangeAxiom& axiom) override { result = heal::Copy(axiom); }
    void visit(const InflowEmptinessAxiom& axiom) override { result = heal::Copy(axiom); }
    void visit(const ObligationAxiom& axiom) override { result = heal::Copy(axiom); }
};

std::unique_ptr<Formula> heal::Copy(const Formula& formula) {
    static FormulaReplicator replicator;
    formula.accept(replicator);
    return std::move(replicator.result);
}

struct TimePredicateReplicator : public BaseLogicVisitor {
    std::unique_ptr<TimePredicate> result;

    void visit(const PastPredicate& predicate) override {
        result = std::make_unique<PastPredicate>(heal::Copy(*predicate.formula));
    }

    void visit(const FuturePredicate& predicate) override {
        result = std::make_unique<FuturePredicate>(heal::Copy(*predicate.pre), predicate.command, heal::Copy(*predicate.post));
    }
};

std::unique_ptr<TimePredicate> heal::Copy(const TimePredicate& predicate) {
    static TimePredicateReplicator replicator;
    predicate.accept(replicator);
    return std::move(replicator.result);
}
