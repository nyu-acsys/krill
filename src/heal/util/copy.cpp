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

std::unique_ptr<LogicVariable> heal::Copy(const LogicVariable& expression) {
    return std::make_unique<LogicVariable>(expression.Decl());
}

std::unique_ptr<SeparatingConjunction> heal::Copy(const SeparatingConjunction& formula) {
    return std::make_unique<SeparatingConjunction>(CopyAll(formula.conjuncts));
}

std::unique_ptr<FlatSeparatingConjunction> heal::Copy(const FlatSeparatingConjunction& formula) {
    return std::make_unique<FlatSeparatingConjunction>(CopyAll(formula.conjuncts));
}

std::unique_ptr<StackAxiom> heal::Copy(const StackAxiom& formula) {
    return std::make_unique<StackAxiom>(heal::Copy(*formula.lhs), formula.op, heal::Copy(*formula.rhs));
}

std::unique_ptr<Annotation> heal::Copy(const Annotation& annotation) {
    return std::make_unique<Annotation>(heal::Copy(*annotation.now), CopyAll(annotation.time));
}

struct StackExpressionReplicator : public BaseLogicVisitor {
    std::unique_ptr<StackExpression> result;

    void visit(const LogicVariable& expression) override { result = heal::Copy(expression); }
    void visit(const SymbolicBool& expression) override { result = std::make_unique<SymbolicBool>(expression.value); }
    void visit(const SymbolicNull& /*expression*/) override { result = std::make_unique<SymbolicNull>(); }
    void visit(const SymbolicMin& /*expression*/) override { result = std::make_unique<SymbolicMin>(); }
    void visit(const SymbolicMax& /*expression*/) override { result = std::make_unique<SymbolicMax>(); }

};

std::unique_ptr<StackExpression> heal::Copy(const StackExpression& expression) {
    static StackExpressionReplicator replicator;
    expression.accept(replicator);
    return std::move(replicator.result);
}

struct AxiomReplicator : public BaseLogicVisitor {
    std::unique_ptr<Axiom> result;

    void visit(const StackAxiom& formula) override {
        result = heal::Copy(formula);
    }
    void visit(const BoolAxiom& formula) override {
        result = std::make_unique<BoolAxiom>(formula.value);
    }
    void visit(const NegatedAxiom& formula) override {
        formula.axiom->accept(*this);
        result = std::make_unique<NegatedAxiom>(std::move(result));
    }
    void visit(const PointsToAxiom& formula) override {
        result = std::make_unique<PointsToAxiom>(heal::Copy(*formula.node), formula.fieldname, heal::Copy(*formula.value));
    }
    void visit(const StackDisjunction& formula) override {
        result = std::make_unique<StackDisjunction>(CopyAll(formula.axioms));
    }
    void visit(const OwnershipAxiom& formula) override {
        result = std::make_unique<OwnershipAxiom>(heal::Copy(*formula.node));
    }
    void visit(const DataStructureLogicallyContainsAxiom& formula) override {
        result = std::make_unique<DataStructureLogicallyContainsAxiom>(heal::Copy(*formula.value));
    }
    void visit(const NodeLogicallyContainsAxiom& formula) override {
        result = std::make_unique<NodeLogicallyContainsAxiom>(heal::Copy(*formula.node), heal::Copy(*formula.value));
    }
    void visit(const KeysetContainsAxiom& formula) override {
        result = std::make_unique<KeysetContainsAxiom>(heal::Copy(*formula.node), heal::Copy(*formula.value));
    }
    void visit(const HasFlowAxiom& formula) override {
        result = std::make_unique<HasFlowAxiom>(heal::Copy(*formula.node));
    }
    void visit(const FlowContainsAxiom& formula) override {
        result = std::make_unique<FlowContainsAxiom>(heal::Copy(*formula.node), heal::Copy(*formula.value_low), heal::Copy(*formula.value_high));
    }
    void visit(const UniqueInflowAxiom& formula) override {
        result = std::make_unique<UniqueInflowAxiom>(heal::Copy(*formula.node));
    }
    void visit(const ObligationAxiom& formula) override {
        result = std::make_unique<ObligationAxiom>(formula.kind, heal::Copy(*formula.key));
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

    void visit(const FlatSeparatingConjunction& formula) override { result = heal::Copy(formula); }
    void visit(const SeparatingConjunction& formula) override { result = heal::Copy(formula); }
    void visit(const SeparatingImplication& formula) override { result = heal::Copy(formula); }
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
        auto assign = std::make_unique<Assignment>(cola::copy(*predicate.command->lhs), cola::copy(*predicate.command->rhs));
        result = std::make_unique<FuturePredicate>(heal::Copy(*predicate.pre), std::move(assign), heal::Copy(*predicate.post));
    }
};

std::unique_ptr<TimePredicate> heal::Copy(const TimePredicate& predicate) {
    static TimePredicateReplicator replicator;
    predicate.accept(replicator);
    return std::move(replicator.result);
}
