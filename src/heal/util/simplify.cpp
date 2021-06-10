#include "heal/util.hpp"

#include "heal/collect.hpp"

using namespace cola;
using namespace heal;


inline const SymbolicVariableDeclaration* ExtractSymbol(const SymbolicExpression& obj) {
    static struct : public DefaultLogicListener {
        const SymbolicVariableDeclaration* result = nullptr;
        void enter(const SymbolicVariable& obj) override { result = &obj.Decl(); }
    } extractor;
    extractor.result = nullptr;
    obj.accept(extractor);
    return extractor.result;
}


//
// Simplify
//

template<typename T>
void Flatten(T& container) {
    // remove nested conjunctions
    std::deque<std::unique_ptr<Formula>> pullDown;
    for (auto& element : container) {
        if (auto conjunction = dynamic_cast<SeparatingConjunction*>(element.get())) {
            std::move(conjunction->conjuncts.begin(), conjunction->conjuncts.end(), std::back_inserter(pullDown));
            element.reset();
        } else if (auto disjunction = dynamic_cast<StackDisjunction*>(element.get())) {
            if (disjunction->axioms.size() == 1) element = std::move(disjunction->axioms.front());
        }
    }
    // remove nullptrs
    container.erase(std::remove_if(container.begin(), container.end(), [](const auto& uptr){
        return uptr.get() == nullptr;
    }), container.end());
    // add pullDown
    std::move(pullDown.begin(), pullDown.end(), std::back_inserter(container));
}

struct Simplifier : public DefaultLogicNonConstListener {
    void exit(SeparatingConjunction& formula) override { Flatten(formula.conjuncts); }
};

void heal::Simplify(LogicObject& object) {
    static Simplifier simplifier;
    object.accept(simplifier);
}


//
// Inline
//

struct EqualityCollector : public BaseResourceVisitor {
    std::deque<std::tuple<const SymbolicVariableDeclaration*, const SymbolicVariableDeclaration*, const SymbolicAxiom*>> equalities;
    void enter(const SymbolicAxiom& axiom) override {
        if (axiom.op != SymbolicAxiom::EQ) return;
        auto lhsSymbol = ExtractSymbol(*axiom.lhs);
        if (!lhsSymbol) return;
        auto rhsSymbol = ExtractSymbol(*axiom.rhs);
        if (!rhsSymbol) return;
        if (lhsSymbol > rhsSymbol) std::swap(lhsSymbol, rhsSymbol);
        equalities.emplace_back(lhsSymbol, rhsSymbol, &axiom);
    }
};

inline void ReplaceSymbol(LogicObject& object, const SymbolicVariableDeclaration& replace, const SymbolicVariableDeclaration& with) {
    struct Replacer : public DefaultLogicNonConstListener { // TODO: really change inline everywhere??
        const SymbolicVariableDeclaration& replace;
        const SymbolicVariableDeclaration& with;
        explicit Replacer(decltype(replace) replace, decltype(with) with) : replace(replace), with(with) {}
        void enter(SymbolicVariable& obj) override {
            if (&obj.Decl() != &replace) return;
            obj.decl_storage = with;
        }
    } visitor(replace, with);
    object.accept(visitor);
}

struct Remover : public BaseNonConstResourceVisitor {
    const SymbolicAxiom& axiom;
    explicit Remover(decltype(axiom) axiom) : axiom(axiom) {}

    template<typename T>
    void Handle(T& container) {
        container.erase(std::remove_if(container.begin(), container.end(), [this](const auto& elem){ return elem.get() == &axiom; }), container.end());
    }
    void enter(SeparatingConjunction& formula) override { Handle(formula.conjuncts); }
    void enter(StackDisjunction& formula) override { Handle(formula.axioms); }
};

inline void RemoveEquality(LogicObject& object, const SymbolicAxiom& axiom) {
    Remover visitor(axiom);
    object.accept(visitor);
}

void heal::InlineAndSimplify(LogicObject& object) {
    EqualityCollector collector;
    object.accept(collector);

    std::set<const SymbolicVariableDeclaration*> removed;
    for (const auto& [decl, other, axiom] : collector.equalities) {
        if (removed.count(decl) != 0) continue;
        ReplaceSymbol(object, *decl, *other);
        RemoveEquality(object, *axiom); // TODO: remove axioms that are trivially true after inlining
        removed.insert(decl);
    }

    heal::Simplify(object);
}