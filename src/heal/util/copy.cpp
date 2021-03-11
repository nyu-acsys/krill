#include "heal/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace heal;


class CopyVisitor : public LogicVisitor {
    std::unique_ptr<Formula> resultFormula;
    std::unique_ptr<TimePredicate> resultPredicate;
    std::unique_ptr<Annotation> resultAnnotation;

    void visit(const ConjunctionFormula& formula) override {
        auto copy = std::make_unique<ConjunctionFormula>();
        for (const auto& conjunct : formula.conjuncts) {
            conjunct->accept(*this);
            copy->conjuncts.push_back(std::move(resultFormula));
        }
        resultFormula = std::move(copy);
    }

    void visit(const ImplicationFormula& formula) override {
        formula.premise->accept(*this);
        auto premise = std::move(resultFormula);
        formula.conclusion->accept(*this);
        auto conclusion = std::move(resultFormula);
        resultFormula = std::make_unique<ImplicationFormula>(std::move(premise), std::move(conclusion));
    }

    void visit(const NegationFormula& formula) override {
        formula.formula->accept(*this);
        resultFormula = std::make_unique<NegationFormula>(std::move(resultFormula));
    }

    void visit(const ExpressionAxiom& formula) override {
        resultFormula = std::make_unique<ExpressionAxiom>(cola::copy(*formula.expr));
    }

    void visit(const OwnershipAxiom& formula) override {
        resultFormula = std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(formula.expr->decl));
    }

    void visit(const DataStructureLogicallyContainsAxiom& formula) override {
        resultFormula = std::make_unique<DataStructureLogicallyContainsAxiom>(cola::copy(*formula.value));
    }

    void visit(const NodeLogicallyContainsAxiom& formula) override {
        resultFormula = std::make_unique<NodeLogicallyContainsAxiom>(cola::copy(*formula.node), cola::copy(*formula.value));
    }

    void visit(const KeysetContainsAxiom& formula) override {
        resultFormula = std::make_unique<KeysetContainsAxiom>(cola::copy(*formula.node), cola::copy(*formula.value));
    }

    void visit(const HasFlowAxiom& formula) override {
        resultFormula = std::make_unique<HasFlowAxiom>(cola::copy(*formula.expr));
    }

    void visit(const FlowContainsAxiom& formula) override {
        resultFormula = std::make_unique<FlowContainsAxiom>(cola::copy(*formula.node), cola::copy(*formula.value_low), cola::copy(*formula.value_high));
    }

    void visit(const UniqueInflowAxiom& formula) override {
        resultFormula = std::make_unique<UniqueInflowAxiom>(cola::copy(*formula.node));
    }

    void visit(const ObligationAxiom& formula) override {
        resultFormula = std::make_unique<ObligationAxiom>(formula.kind, std::make_unique<VariableExpression>(formula.key->decl));
    }

    void visit(const FulfillmentAxiom& formula) override {
        resultFormula = std::make_unique<FulfillmentAxiom>(formula.kind, std::make_unique<VariableExpression>(formula.key->decl), formula.return_value);
    }

    void visit(const PastPredicate& predicate) override {
        predicate.formula->accept(*this);
        resultPredicate = std::make_unique<PastPredicate>(std::move(resultFormula));
    }

    void visit(const FuturePredicate& predicate) override {
        predicate.pre->accept(*this);
        auto pre = std::move(resultFormula);
        predicate.post->accept(*this);
        auto post = std::move(resultFormula);
        auto cmd = std::make_unique<Assignment>(cola::copy(*predicate.command->lhs), cola::copy(*predicate.command->rhs));
        resultPredicate = std::make_unique<FuturePredicate>(std::move(pre), std::move(cmd), std::move(post));
    }

    void visit(const Annotation& annotation) override {
        resultAnnotation = std::make_unique<Annotation>();
        annotation.now->accept(*this);
        resultAnnotation->now = std::move(resultFormula);
        for (const auto& predicate : annotation.time) {
            predicate->accept(*this);
            resultAnnotation->time.push_back(std::move(resultPredicate));
        }
    }

    template<typename T>
    static CopyVisitor Handle(const T& object) {
        CopyVisitor visitor;
        object.accept(visitor);
        return visitor;
    }

    public:
    static std::unique_ptr<Formula> Copy(const Formula& formula) {
        auto visitor = Handle(formula);
        return std::move(visitor.resultFormula);
    }
    static std::unique_ptr<TimePredicate> Copy(const TimePredicate& predicate) {
        auto visitor = Handle(predicate);
        return std::move(visitor.resultPredicate);
    }
    static std::unique_ptr<Annotation> Copy(const Annotation& annotation) {
        auto visitor = Handle(annotation);
        return std::move(visitor.resultAnnotation);
    }
};

std::unique_ptr<Formula> heal::Copy(const Formula& formula) {
    return CopyVisitor::Copy(formula);
}

std::unique_ptr<TimePredicate> heal::Copy(const TimePredicate& predicate) {
    return CopyVisitor::Copy(predicate);
}

std::unique_ptr<Annotation> heal::Copy(const Annotation& annotation) {
    return CopyVisitor::Copy(annotation);
}
