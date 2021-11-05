#include "logics/util.hpp"

#include <set>
#include <sstream>
#include "util/shortcuts.hpp"

using namespace plankton;


struct SymbolReplacementListener : public MutableLogicListener {
    const SymbolDeclaration& search;
    const SymbolDeclaration& replace;
    
    explicit SymbolReplacementListener(const SymbolDeclaration& search, const SymbolDeclaration& replace)
            : search(search), replace(replace) {}
    
    void Enter(SymbolicVariable& object) override {
        if (object.Decl() != search) return;
        object.decl = replace;
    }
};

inline void ApplyRenaming(LogicObject& object, const SymbolDeclaration& search, const SymbolDeclaration& replace) {
    SymbolReplacementListener replacer(search, replace);
    object.Accept(replacer);
}

//
// Flatten
//

struct FlattenVisitor final : public MutableDefaultLogicVisitor {
    
    void Visit(SeparatingConjunction& object) override {
        // find all nested separating conjunctions
        std::set<SeparatingConjunction*> pullUp;
        for (auto& conjunct : object.conjuncts) {
            conjunct->Accept(*this);
    
            if (auto conjunction = dynamic_cast<SeparatingConjunction*>(conjunct.get())) {
                pullUp.insert(conjunction);
            }
        }
        
        // flatten
        for (auto& conjunction : pullUp) MoveInto(std::move(conjunction->conjuncts), object.conjuncts);
        RemoveIf(object.conjuncts, [&pullUp](const auto& elem) { return plankton::Membership(pullUp, elem.get()); });
    }
    
    void Visit(NonSeparatingImplication& object) override { Walk(object); }
    void Visit(ImplicationSet& object) override { Walk(object); }
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
    void Visit(NonSeparatingImplication& object) override {
        VisitWithinNewContext(object, *object.premise);
        VisitWithinNewContext(*object.conclusion, *object.conclusion);
    }
    void Visit(PastPredicate& object) override {
        VisitWithinNewContext(*object.formula, *object.formula);
    }
};

inline void InlineEqualities(LogicObject& object) {
    SyntacticEqualityInliningListener inlining(object);
    object.Accept(inlining);
}


//
// Inline memories
//

// struct EqualityCollection {
//     std::set<std::pair<const SymbolDeclaration*, const SymbolDeclaration*>> set;
//
//     void Add(const SymbolDeclaration& symbol, const SymbolDeclaration& other) {
//         assert(symbol.type == other.type);
//         assert(symbol.order == other.order);
//         if (&symbol <= &other) set.emplace(&symbol, &other);
//         else set.emplace(&other, &symbol);
//     }
//
//     void Handle(const SharedMemoryCore& memory, SharedMemoryCore& other) {
//         if (memory.node->Decl() != other.node->Decl()) return;
//         Add(memory.node->Decl(), other.node->Decl());
//         Add(memory.flow->Decl(), other.flow->Decl());
//         assert(memory.node->Type() == other.node->Type());
//         for (const auto& [field, value] : memory.fieldToValue) {
//             Add(value->Decl(), other.fieldToValue.at(field)->Decl());
//         }
//     }
// };
//
// inline EqualityCollection ExtractEqualities(SeparatingConjunction& object) {
//     EqualityCollection equalities;
//     auto memories = plankton::CollectMutable<SharedMemoryCore>(object);
//     for (auto it = memories.begin(); it != memories.end(); ++it) {
//         for (auto ot = std::next(it); ot != memories.end(); ++ot) {
//             equalities.Handle(**it, **ot);
//         }
//     }
//     return equalities;
// }
//
// #include "util/log.hpp"
// inline void InlineMemories(SeparatingConjunction& object) {
//     // TODO: this breaks when there are more than 2 copies of a memory
//     auto equalities = ExtractEqualities(object);
//     for (const auto& [symbol, other] : equalities.set) {
//         DEBUG(" inlining " << symbol->name << " ~> " << other->name << std::endl)
//         ApplyRenaming(object, *symbol, *other);
//     }
// }

struct MemoryCollector : public MutableLogicListener {
    std::map<const SymbolDeclaration*, std::set<SharedMemoryCore*>> memories;
    void Enter(SharedMemoryCore& object) override {
        memories[&object.node->Decl()].insert(&object);
    }
};

inline std::unique_ptr<Axiom> MakeEq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
    return std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(decl), std::make_unique<SymbolicVariable>(other));
}

inline void MakeMemoriesEqual(const SharedMemoryCore& src, SharedMemoryCore& other, SeparatingConjunction& object) {
    assert(src.node->Decl() == other.node->Decl());
    object.Conjoin(MakeEq(other.flow->Decl(), src.flow->Decl()));
    other.flow->decl = src.flow->Decl();
    for (auto& [field, value] : other.fieldToValue) {
        auto& newValue = src.fieldToValue.at(field)->Decl();
        object.Conjoin(MakeEq(value->decl, newValue));
        value->decl = newValue;
    }
}

inline void InlineMemories(SeparatingConjunction& object) {
    MemoryCollector collector;
    object.Accept(collector);
    bool inlined = false;
    for (const auto& pair : collector.memories) {
        auto& equalSet = pair.second;
        if (equalSet.empty()) continue;
        for (auto it = std::next(equalSet.begin()); it != equalSet.end(); ++it) {
            MakeMemoriesEqual(**equalSet.begin(), **it, object);
            inlined = true;
        }
    }
    if (inlined) InlineEqualities(object);
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
    
        RemoveIf(object.conjuncts, [](const auto& elem){ return !elem; });
    }
    
    void Visit(NonSeparatingImplication& object) override { Walk(object); }
    void Visit(ImplicationSet& object) override { Walk(object); }
    void Visit(PastPredicate& object) override { Walk(object); }
    void Visit(FuturePredicate& object) override { Walk(object); }

    template<typename T>
    inline void RemoveDuplicates(T& container) {
        for (auto it = container.begin(); it != container.end(); ++it) {
            if (!*it) continue;
            for (auto ot = std::next(it); ot != container.end(); ++ot) {
                if (!*ot) continue;
                if (!plankton::SyntacticalEqual(**it, **ot)) continue;
                ot->reset(nullptr);
            }
        }
        plankton::RemoveIf(container, [](const auto& elem){ return !elem; });
    }

    void Visit(Annotation& object) override {
        Walk(object);
        RemoveDuplicates(object.past);
        RemoveDuplicates(object.future);
    }
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

void plankton::InlineAndSimplify(Annotation& object) {
    Flatten(object);
    InlineEqualities(object);
    InlineMemories(*object.now);
    RemoveNoise(object);
}
