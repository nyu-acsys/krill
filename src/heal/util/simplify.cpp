#include "heal/util.hpp"

using namespace cola;
using namespace heal;


inline BinaryExpression::Operator negatedOp(BinaryExpression::Operator op) {
    switch (op) {
        case BinaryExpression::Operator::EQ: return BinaryExpression::Operator::NEQ;
        case BinaryExpression::Operator::NEQ: return BinaryExpression::Operator::EQ;
        case BinaryExpression::Operator::LEQ: return BinaryExpression::Operator::GT;
        case BinaryExpression::Operator::LT: return BinaryExpression::Operator::GEQ;
        case BinaryExpression::Operator::GEQ: return BinaryExpression::Operator::LT;
        case BinaryExpression::Operator::GT: return BinaryExpression::Operator::LEQ;
        default:
            throw std::logic_error("Cannot negate " + cola::toString(op) + "."); // TODO: custom error subclass
    }
}

std::unique_ptr<cola::Expression> heal::Simplify(std::unique_ptr<cola::Expression> expression) {
    if (auto negation = dynamic_cast<NegatedExpression*>(expression.get())) {
        if (auto boolean = dynamic_cast<BooleanValue*>(negation->expr.get())) {
            return MakeBoolExpr(!boolean->value);
        }
        if (auto binary = dynamic_cast<BinaryExpression*>(negation->expr.get())) {
            if (binary->op == BinaryExpression::Operator::OR) {
                binary->lhs = heal::Simplify(std::move(binary->lhs));
                binary->rhs = heal::Simplify(std::move(binary->rhs));
                return std::move(negation->expr);
            } else if (binary->op != BinaryExpression::Operator::AND) {
                binary->op = negatedOp(binary->op);
                return std::move(negation->expr);
            }
        }
    }
    return expression;
}

std::unique_ptr<Formula> FlattenExpression(std::unique_ptr<Expression> expression) {
    expression = heal::Simplify(std::move(expression));
    if (auto negation = dynamic_cast<NegatedExpression*>(expression.get())) {
        return heal::Simplify(heal::MakeNegation(FlattenExpression(std::move(negation->expr))));
    }
    if (auto binary = dynamic_cast<BinaryExpression*>(expression.get())) {
        if (binary->op == BinaryExpression::Operator::AND) {
            return heal::Simplify(heal::MakeConjunction(FlattenExpression(std::move(binary->lhs)), FlattenExpression(std::move(binary->rhs))));
        }
    }
    return heal::MakeAxiom(std::move(expression));
}

struct FormulaSimplifier : public DefaultNonConstListener {
    std::unique_ptr<Formula> result;

    static const ExpressionAxiom& True() {
        static ExpressionAxiom trueAxiom = ExpressionAxiom(heal::MakeTrueExpr());
        return trueAxiom;
    }

    static const ExpressionAxiom& False() {
        static ExpressionAxiom falseAxiom = ExpressionAxiom(heal::MakeFalseExpr());
        return falseAxiom;
    }

    static std::unique_ptr<ExpressionAxiom> MakeTrue() {
        return heal::MakeAxiom(heal::MakeTrueExpr());
    }

    static std::unique_ptr<ExpressionAxiom> MakeFalse() {
        return heal::MakeAxiom(heal::MakeFalseExpr());
    }

    static bool IsTrue(const std::unique_ptr<Formula>& obj) {
        return heal::SyntacticallyEqual(*obj, True());
    }

    static bool IsFalse(const std::unique_ptr<Formula>& obj) {
        return heal::SyntacticallyEqual(*obj, False());
    }

    void enter(ExpressionAxiom& formula) override {
        assert(&formula == result.get());
        result = FlattenExpression(std::move(formula.expr));
    }

    void enter(DataStructureLogicallyContainsAxiom& formula) override {
        assert(&formula == result.get());
        formula.value = heal::Simplify(std::move(formula.value));
    }

    void enter(NodeLogicallyContainsAxiom& formula) override {
        assert(&formula == result.get());
        formula.node = heal::Simplify(std::move(formula.node));
        formula.value = heal::Simplify(std::move(formula.value));
    }

    void enter(KeysetContainsAxiom& formula) override {
        assert(&formula == result.get());
        formula.node = heal::Simplify(std::move(formula.node));
        formula.value = heal::Simplify(std::move(formula.value));
    }

    void enter(HasFlowAxiom& formula) override {
        assert(&formula == result.get());
        formula.expr = heal::Simplify(std::move(formula.expr));
    }

    void enter(FlowContainsAxiom& formula) override {
        assert(&formula == result.get());
        formula.node = heal::Simplify(std::move(formula.node));
        formula.value_low = heal::Simplify(std::move(formula.value_low));
        formula.value_high = heal::Simplify(std::move(formula.value_high));
    }

    void enter(UniqueInflowAxiom& formula) override {
        assert(&formula == result.get());
        formula.node = heal::Simplify(std::move(formula.node));
    }

    void exit(ImplicationFormula& formula) override {
        assert(&formula == result.get());
        if (IsFalse(formula.premise)) result = MakeTrue();
        else if (IsTrue(formula.conclusion)) result = MakeTrue();
        else if (IsTrue(formula.premise)) result = std::move(formula.conclusion);
        else if (IsFalse(formula.conclusion)) result = std::make_unique<NegationFormula>(std::move(formula.premise));
    }

    void exit(ConjunctionFormula& formula) override {
        assert(&formula == result.get());

        // false conjunct makes entire formula false
        if (formula.conjuncts.end() != std::find_if(formula.conjuncts.begin(), formula.conjuncts.end(), IsFalse)) {
            result = MakeFalse();
            return;
        }

        // flatten nested conjunctions
        std::deque<std::unique_ptr<Formula>> pullOut;
        for (auto& conjunct : formula.conjuncts) {
            if (auto nestedConjunction = dynamic_cast<ConjunctionFormula*>(conjunct.get())) {
                std::move(nestedConjunction->conjuncts.begin(), nestedConjunction->conjuncts.end(), std::back_inserter(pullOut));
                conjunct = MakeTrue();
            }
        }
        std::move(pullOut.begin(), pullOut.end(), std::back_inserter(formula.conjuncts));

        // remove duplicates
        for (auto iter = formula.conjuncts.begin(); iter != formula.conjuncts.end(); ++iter) {
            for (auto other = std::next(iter); other != formula.conjuncts.end(); ++iter) {
                if (heal::SyntacticallyEqual(**iter, **other)) *other = MakeTrue();
            }
        }

        // true conjuncts are redundant
        formula.conjuncts.erase(std::remove_if(formula.conjuncts.begin(), formula.conjuncts.end(), IsTrue), formula.conjuncts.end());

        // no conjuncts means true
        if (formula.conjuncts.empty()) result = MakeTrue();

        // one conjunct means simple formula
        if (formula.conjuncts.size() == 1) result = std::move(formula.conjuncts.front());
    }

    void exit(NegationFormula& formula) override {
        assert(&formula == result.get());
        if (IsTrue(formula.formula)) result = MakeFalse();
        else if (IsFalse(formula.formula)) result = MakeTrue();
        else if (auto implication = dynamic_cast<ImplicationFormula*>(formula.formula.get())) {
            // rewrite negated implication into a conjunction
            result = heal::MakeConjunction(std::move(implication->premise), heal::MakeNegation(std::move(implication->conclusion)));
        } else if (auto negation = dynamic_cast<NegationFormula*>(formula.formula.get())) {
            // remove double negation
            result = std::move(negation->formula);
        }
    }

    std::unique_ptr<Formula> Simplify(std::unique_ptr<Formula> formula) {
        auto uPtr = std::move(result); // allow recursive decent
        result = std::move(formula);
        result->accept(*this);
        result.swap(uPtr);
        return uPtr;
    }
};

std::unique_ptr<Formula> heal::Simplify(std::unique_ptr<Formula> formula) {
    static FormulaSimplifier simplifier;
    return simplifier.Simplify(std::move(formula));
}

struct TimePredicateSimplifier : public DefaultNonConstListener {
    void enter(PastPredicate& predicate) override {
        predicate.formula = heal::Simplify(std::move(predicate.formula));
    }

    void enter(FuturePredicate& predicate) override {
        predicate.pre = heal::Simplify(std::move(predicate.pre));
        predicate.post = heal::Simplify(std::move(predicate.post));
    }
};

std::unique_ptr<TimePredicate> heal::Simplify(std::unique_ptr<TimePredicate> predicate) {
    static TimePredicateSimplifier simplifier;
    predicate->accept(simplifier);
    return predicate;
}

std::unique_ptr<Annotation> heal::Simplify(std::unique_ptr<Annotation> annotation) {
    auto result = std::make_unique<Annotation>(Simplify(std::move(annotation->now)));
    for (auto& predicate : annotation->time) {
        result->time.push_back(Simplify(std::move(predicate)));
    }
    return result;
}
