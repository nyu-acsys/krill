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

inline void ApplyRenaming(LogicObject& object, const SymbolDeclaration& search, const SymbolDeclaration& replace) {
    SymbolRenamingListener replacer(search, replace);
    object.Accept(replacer);
}

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
    void Visit(Invariant& object) override { Walk(object); }
    void Visit(PastPredicate& object) override { Walk(object); }
    void Visit(FuturePredicate& object) override { Walk(object); }
    void Visit(Annotation& object) override { Walk(object); }
};

inline void Flatten(LogicObject& object) {
    FlattenVisitor visitor;
    object.Accept(visitor);
}


//
// Inline memories
//

struct EqualityCollection {
    std::set<std::pair<const SymbolDeclaration*, const SymbolDeclaration*>> set;
    
    void Add(const SymbolDeclaration& symbol, const SymbolDeclaration& other) {
        assert(symbol.type == other.type);
        assert(symbol.order == other.order);
        if (&symbol <= &other) set.emplace(&symbol, &other);
        else set.emplace(&other, &symbol);
    }
    
    void Handle(const SharedMemoryCore& memory, SharedMemoryCore& other) {
        if (memory.node->Decl() != other.node->Decl()) return;
        Add(memory.node->Decl(), other.node->Decl());
        Add(memory.flow->Decl(), other.flow->Decl());
        assert(memory.node->Type() == other.node->Type());
        for (const auto& [field, value] : memory.fieldToValue) {
            Add(value->Decl(), other.fieldToValue.at(field)->Decl());
        }
    }
};

inline std::set<SharedMemoryCore*> GetMemories(LogicObject& object) {
    // TODO: how to avoid this?
    if (auto annotation = dynamic_cast<Annotation*>(&object))
        return plankton::CollectMutable<SharedMemoryCore>(*annotation->now);
    return plankton::CollectMutable<SharedMemoryCore>(object);
}

inline void ExtractEqualities(SeparatingConjunction& object, EqualityCollection& equalities) {
    auto memories = GetMemories(object);
    for (auto it = memories.begin(); it != memories.end(); ++it) {
        for (auto ot = std::next(it); ot != memories.end(); ++ot) {
            equalities.Handle(**it, **ot);
        }
    }
}

inline void InlineMemories(LogicObject& object) {
    // find equalities
    struct : public MutableDefaultLogicVisitor {
        EqualityCollection equalities;
        
        void Visit(SeparatingConjunction& object) override { ExtractEqualities(object, equalities); }
        void Visit(Annotation& object) override { Walk(object); }
        
        inline static void Raise(const std::string& typeName) {
            throw std::logic_error("Cannot simplify '" + typeName + "'");// TODO: better error handling
        }
        void Visit(SeparatingImplication& /*object*/) override { Raise("SeparatingImplication"); }
        void Visit(FuturePredicate& /*object*/) override { Raise("FuturePredicate"); }
    } dispatcher;
    object.Accept(dispatcher);
    
    // inline equalities
    for (const auto& [symbol, other] : dispatcher.equalities.set) {
        ApplyRenaming(object, *symbol, *other);
    }
}


//
// Inline Equalities
//

struct SyntacticEqualityInliningListener final : public MutableLogicListener {
    std::reference_wrapper<LogicObject> target;
    explicit SyntacticEqualityInliningListener(LogicObject& target) : target(target) {}
    
    void Enter(StackAxiom& object) override {
        if (object.op != BinaryOperator::EQ) return;
        if (auto lhsVar = dynamic_cast<const SymbolicVariable*>(object.lhs.get())) {
            if (auto rhsVar = dynamic_cast<const SymbolicVariable*>(object.rhs.get())) {
                ApplyRenaming(target, lhsVar->Decl(), rhsVar->Decl());
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
    void Visit(Invariant& object) override { Walk(object); }
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
    // TODO: should this be done for annotations only?
    Flatten(object);
    InlineMemories(object);
    InlineEqualities(object);
    RemoveNoise(object);
}
