#include "logics/util.hpp"

#include <set>
#include <sstream>
#include "util/shortcuts.hpp"

using namespace plankton;

// simplification:
//  1. flatten
//  2. dupletten entferenen
//  3. gleichheiten inlinen
//  4. trivally true formeln entfernen


//
// Flatten
//

struct FlattenVisitor final : public MutableDefaultLogicVisitor {
    std::unique_ptr<SeparatingConjunction>* nested = nullptr;
    
    void Visit(SeparatingConjunction& object) override {
        assert(!nested);
        
        // find all nested separating conjunctions
        std::deque<std::unique_ptr<SeparatingConjunction>> pulledUp;
        for (auto& conjunct : object.conjuncts) {
            conjunct->Accept(*this);
            if (!nested) continue;
            assert(conjunct.get() == nested->get());
            pulledUp.push_back(std::move(*nested));
            nested->reset(nullptr);
        }
        
        // flatten
        RemoveIf(object.conjuncts, [](const auto& elem){ return elem.get() == nullptr; });
        for (auto& conjunction : pulledUp) MoveInto(std::move(conjunction->conjuncts), object.conjuncts);
    }
    
    void Visit(SeparatingImplication& object) override { Walk(object); }
    void Visit(PastPredicate& object) override { Walk(object); }
    void Visit(FuturePredicate& object) override { Walk(object); }
    void Visit(Annotation& object) override { Walk(object); }
};

inline void Flatten(LogicObject& object) {
    FlattenVisitor visitor;
    object.Accept(visitor);
}


//
// Inline Equalities
//

struct SymbolRenamingListener : public MutableLogicListener {
    const SymbolDeclaration& search;
    const SymbolDeclaration& replace;
    
    explicit SymbolRenamingListener(const SymbolDeclaration& search, const SymbolDeclaration& replace)
            : search(search), replace(replace) {}
    
    void Enter(SymbolicVariable& object) override {
        if (&object.Decl() != &search) return;
        object.decl = replace;
    }
};

struct SyntacticEqualityInliningListener final : public MutableLogicListener {
    std::reference_wrapper<LogicObject> target;
    explicit SyntacticEqualityInliningListener(LogicObject& target) : target(target) {}
    
    void Enter(StackAxiom& object) override {
        if (object.op != BinaryOperator::EQ) return;
        if (auto lhsVar = dynamic_cast<const SymbolicVariable*>(object.lhs.get())) {
            if (auto rhsVar = dynamic_cast<const SymbolicVariable*>(object.rhs.get())) {
                SymbolRenamingListener renaming(lhsVar->Decl(), rhsVar->Decl());
                target.get().Accept(renaming);
            }
        }
    }
    
    // do not inline across multiple contexts
    inline void VisitWithinNewContext(LogicObject& newContext, LogicObject& toVisit) {
        auto& oldTarget = target;
        target = newContext;
        toVisit.Accept(*this);
        target = oldTarget;
    }
    void Visit(SeparatingImplication& object) override {
        VisitWithinNewContext(object, *object.premise);
        VisitWithinNewContext(*object.conclusion, *object.conclusion);
    }
    void Visit(PastPredicate& object) override {
        VisitWithinNewContext(*object.formula, *object.formula);
    }
    void Visit(FuturePredicate& object) override {
        VisitWithinNewContext(*object.pre, *object.pre);
        VisitWithinNewContext(*object.post, *object.post);
    }
};

inline void InlineEqualities(LogicObject& object) {
    SyntacticEqualityInliningListener inlining(object);
    object.Accept(inlining);
}


//
// Remove trivialities and duplicates
//

struct CleanUpVisitor final : public MutableDefaultLogicVisitor {
    bool removeAxiom = false;
    
    void Visit(StackAxiom& object) override {
        if (object.op != BinaryOperator::EQ) return;
        if (!plankton::SyntacticalEqual(*object.lhs, *object.rhs)) return;
        removeAxiom = true;
    }
    
    void Visit(SeparatingConjunction& object) override {
        assert(!removeAxiom);
        
        for (auto it = object.conjuncts.begin(); it != object.conjuncts.end(); ++it) {
            (*it)->Accept(*this);
            if (removeAxiom) {
                it->reset(nullptr);
                removeAxiom = false;
                continue;
            }
            for (auto other = std::next(it); other != object.conjuncts.end(); ++other) {
                if (!plankton::SyntacticalEqual(**it, **other)) continue;
                it->reset(nullptr);
                break;
            }
        }
    
        RemoveIf(object.conjuncts, [](const auto& elem){ return elem.get() == nullptr; });
    }
    
    void Visit(SeparatingImplication& object) override { Walk(object); }
    void Visit(PastPredicate& object) override { Walk(object); }
    void Visit(FuturePredicate& object) override { Walk(object); }
    void Visit(Annotation& object) override { Walk(object); }
};

inline void RemoveNoise(LogicObject& object) {
    CleanUpVisitor visitor;
    object.Accept(visitor);
}


//
// Overall algorithms
//

void plankton::Simplify(LogicObject& object) {
    Flatten(object);
}

void plankton::InlineAndSimplify(LogicObject& object) {
    Flatten(object);
    InlineEqualities(object);
    RemoveNoise(object);
}
