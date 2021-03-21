#include "heal/util.hpp"

using namespace cola;
using namespace heal;


struct ConstantChecker : public BaseLogicVisitor {
    enum VALUE { TRUE, FALSE, UNKNOWN };
    VALUE result;

    static inline VALUE Invert(VALUE value) {
        switch (value) {
            case TRUE: return FALSE;
            case FALSE: return TRUE;
            case UNKNOWN: return UNKNOWN;
        }
    }

    static inline VALUE And(VALUE value, VALUE other) {
        switch (value) {
            case TRUE: return other;
            case FALSE: return other == UNKNOWN ? UNKNOWN : FALSE;
            case UNKNOWN: return UNKNOWN;
        }
    }

    static inline VALUE Or(VALUE value, VALUE other) {
        switch (value) {
            case TRUE: return other == UNKNOWN ? UNKNOWN : TRUE;
            case FALSE: return other;
            case UNKNOWN: return UNKNOWN;
        }
    }

    template<typename T, typename C>
    void HandleConjunction(const T& elements, const C& combine, VALUE init) {
        VALUE oldResult = init;
        for (const auto& conjunct : elements) {
            conjunct->accept(*this);
            result = combine(oldResult, result);
            if (result == UNKNOWN) break;
        }
    }

    void visit(const FlatSeparatingConjunction& formula) override {
        HandleConjunction(formula.conjuncts, And, TRUE);
    }

    void visit(const SeparatingConjunction& formula) override {
        HandleConjunction(formula.conjuncts, And, TRUE);
    }

    void visit(const StackDisjunction& axiom) override {
        HandleConjunction(axiom.axioms, Or, FALSE);
    }

    void visit(const SeparatingImplication& formula) override {
        formula.premise->accept(*this);
        if (result == FALSE) {
            result = TRUE;
            return;
        }
        formula.conclusion->accept(*this);
        if (result != TRUE) {
            result = UNKNOWN;
        }
    }

    void visit(const NegatedAxiom& axiom) override {
        axiom.axiom->accept(*this);
        result = Invert(result);
    }

    void visit(const StackAxiom& axiom) override {
        if (heal::SyntacticallyEqual(*axiom.lhs, *axiom.rhs)) {
            switch (axiom.op) {
                case StackAxiom::LEQ:
                case StackAxiom::GEQ:
                case StackAxiom::EQ:
                    result = TRUE;
                case StackAxiom::NEQ:
                case StackAxiom::LT:
                case StackAxiom::GT:
                    result = FALSE;
            }
        } else {
            // TODO: there are more cases that can be detected syntactically (involving only constants)
            result = UNKNOWN;
        }
    }

    void visit(const BoolAxiom& axiom) override {
        result = axiom.value ? TRUE : FALSE;
    }

    void visit(const PointsToAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const OwnershipAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const DataStructureLogicallyContainsAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const NodeLogicallyContainsAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const KeysetContainsAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const HasFlowAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const FlowContainsAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const UniqueInflowAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const ObligationAxiom& /*formula*/) override { result = UNKNOWN; }
    void visit(const FulfillmentAxiom& /*formula*/) override { result = UNKNOWN; }
};

static bool IsTrue(const LogicObject& object) {
    static ConstantChecker checker;
    object.accept(checker);
    return checker.result == ConstantChecker::TRUE;
}

static bool IsFalse(const LogicObject& object) {
    static ConstantChecker checker;
    object.accept(checker);
    return checker.result == ConstantChecker::FALSE;
}

static StackAxiom::Operator NegateOperator(StackAxiom::Operator op) {
    switch (op) {
        case StackAxiom::EQ: return StackAxiom::NEQ;
        case StackAxiom::NEQ: return StackAxiom::EQ;
        case StackAxiom::LEQ: return StackAxiom::GT;
        case StackAxiom::LT: return StackAxiom::GEQ;
        case StackAxiom::GEQ: return StackAxiom::LT;
        case StackAxiom::GT: return StackAxiom::LEQ;
    }
}

template<typename T>
void FlattenAxiom(std::unique_ptr<T>& axiom, bool rewriteBool=true) {
    if (auto negation = dynamic_cast<NegatedAxiom*>(axiom.get())) {
        if (auto stack = dynamic_cast<StackAxiom *>(negation->axiom.get())) {
            axiom = heal::MakeStackAxiom(std::move(stack->lhs), NegateOperator(stack->op), std::move(stack->rhs));

        } else if (auto subnegation = dynamic_cast<NegatedAxiom *>(negation->axiom.get())) {
            axiom = std::move(subnegation->axiom);
        }

    } else if (auto disjunction = dynamic_cast<StackDisjunction*>(axiom.get())) {
        if (disjunction->axioms.size() == 1) {
            axiom = std::move(disjunction->axioms.front());
        }

    } else if (rewriteBool && IsTrue(*axiom)) {
        axiom = heal::MakeBoolAxiom(true);
    } else if (rewriteBool && IsFalse(*axiom)) {
        axiom = heal::MakeBoolAxiom(false);
    }
}

template<typename T>
void FlattenFlatFormula(std::unique_ptr<T>& formula) {
    FlattenAxiom(formula, false);

    if (auto conjunction = dynamic_cast<FlatSeparatingConjunction*>(formula.get())) {
        if (conjunction->conjuncts.empty()) formula = heal::MakeBoolAxiom(true);
        else if (conjunction->conjuncts.size() == 1) formula = std::move(conjunction->conjuncts.front());

    } else if (auto negation = dynamic_cast<NegatedAxiom*>(formula.get())) {
        if (auto disjunction = dynamic_cast<StackDisjunction*>(negation->axiom.get())) {
            auto result = std::make_unique<FlatSeparatingConjunction>();
            std::move(disjunction->axioms.begin(), disjunction->axioms.end(), std::back_inserter(result->conjuncts));
            formula = std::move(result);
        }
    }
}

template<typename T>
void FlattenFormula(std::unique_ptr<T>& formula) {
    FlattenFlatFormula(formula);

    if (auto implication = dynamic_cast<SeparatingImplication*>(formula.get())) {
        if (IsTrue(*implication->premise)) { formula = std::move(implication->conclusion); }
        else if (IsFalse(*implication->premise)) { formula = heal::MakeBoolAxiom(true); }
        else if (IsTrue(*implication->conclusion)) { formula = heal::MakeBoolAxiom(true); }
        else if (IsFalse(*implication->conclusion)) { implication->conclusion = heal::MakeBoolAxiom(false); } // cannot encode negation of implication->premise
    }
}

void FlattenConjunction(std::deque<std::unique_ptr<Formula>>& container) {
    for (auto& element : container) {
        FlattenFormula(element);
    }

    // remove nested conjunctions
    std::deque<std::unique_ptr<Formula>> pullDown;
    for (auto& element : container) {
        if (auto conjunction = dynamic_cast<SeparatingConjunction*>(element.get())) {
            std::move(conjunction->conjuncts.begin(), conjunction->conjuncts.end(), std::back_inserter(pullDown));
            element = heal::MakeBoolAxiom(true);
        } else if (auto flatConjunction = dynamic_cast<FlatSeparatingConjunction*>(element.get())) {
            std::move(flatConjunction->conjuncts.begin(), flatConjunction->conjuncts.end(), std::back_inserter(pullDown));
            element = heal::MakeBoolAxiom(true);
        }
    }
}

template<typename T>
void RemoveDuplicates(std::deque<std::unique_ptr<T>>& container) {
    // reset duplicates
    for (auto it = container.begin(); it != container.end(); ++it) {
        for (auto other = std::next(it); other != container.end(); ++other) {
            if (heal::SyntacticallyEqual(**it, **other)) {
                other->reset();
            }
        }
    }

    // remove duplicates
    container.erase(std::remove_if(container.begin(), container.end(), [](const auto& uptr)->bool{
        return uptr.get() != nullptr;
    }), container.end());
}

template<typename T>
void RemoveElementIf(std::deque<std::unique_ptr<T>>& container, const std::function<bool(const LogicObject&)>& predicate) {
    container.erase(std::remove_if(container.begin(), container.end(), [&predicate](const auto& uptr){
        return predicate(*uptr);
    }), container.end());
}

template<typename T>
[[nodiscard]] bool ContainsElementIf(std::deque<std::unique_ptr<T>>& container, const std::function<bool(const LogicObject&)>& predicate) {
    return container.end() != std::find_if(container.begin(), container.end(), [&predicate](const auto& uptr){
        return predicate(*uptr);
    });
}

struct Simplifier : public DefaultLogicNonConstListener {
    template<typename T>
    void ExitConjunction(T& formula) {
        RemoveDuplicates(formula.conjuncts);
        RemoveElementIf(formula.conjuncts, IsTrue);
        if (formula.conjuncts.empty()) {
            formula.conjuncts.push_back(heal::MakeBoolAxiom(true));
        } else if (ContainsElementIf(formula.conjuncts, IsFalse)) {
            formula.conjuncts.clear();
            formula.conjuncts.push_back(heal::MakeBoolAxiom(false));
        }
    }
    void exit(SeparatingConjunction& formula) override { FlattenConjunction(formula.conjuncts); ExitConjunction(formula); }
    void exit(FlatSeparatingConjunction& formula) override { ExitConjunction(formula); }

    void exit(SeparatingImplication& formula) override {
        FlattenFlatFormula(formula.premise);
        FlattenFlatFormula(formula.conclusion);
    }

    void exit(StackDisjunction& axiom) override {
        RemoveDuplicates(axiom.axioms);
        RemoveElementIf(axiom.axioms, IsFalse);
        if (axiom.axioms.empty()) {
            axiom.axioms.push_back(heal::MakeStackAxiom(heal::MakeMin(), StackAxiom::NEQ, heal::MakeMin())); // add false axiom
        } else if (ContainsElementIf(axiom.axioms, IsTrue)) {
            axiom.axioms.clear();
            axiom.axioms.push_back(heal::MakeStackAxiom(heal::MakeMin(), StackAxiom::EQ, heal::MakeMin())); // add true axiom
        }
    }

    void exit(NegatedAxiom& axiom) override {
        FlattenAxiom(axiom.axiom);
    }

     void exit(PastPredicate& predicate) override {
         FlattenFormula(predicate.formula);
    }

     void exit(FuturePredicate& predicate) override {
         FlattenFormula(predicate.pre);
         FlattenFormula(predicate.post);
    }
};


void heal::Simplify(LogicObject& object) {
    static Simplifier simplifier;
    object.accept(simplifier);
}
