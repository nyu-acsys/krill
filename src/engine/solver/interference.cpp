#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;

using EffectPairDeque = std::deque<std::pair<const HeapEffect*, const HeapEffect*>>;


//
// Implication among effects
//

inline bool CheckContext(const Formula& context) {
    struct : public LogicListener {
        bool result = true;
        void Enter(const LocalMemoryResource& /*object*/) override { result = false; }
        void Enter(const SharedMemoryCore& /*object*/) override { result = false; }
        // void Enter(const EqualsToAxiom& /*object*/) override { result = false; }
        void Enter(const ObligationAxiom& /*object*/) override { result = false; }
        void Enter(const FulfillmentAxiom& /*object*/) override { result = false; }
    } visitor;
    context.Accept(visitor);
    return visitor.result;
}

inline bool UpdateSubset(const HeapEffect& premise, const HeapEffect& conclusion) {
    if (plankton::UpdatesFlow(conclusion) && !plankton::UpdatesFlow(premise)) return false;
    const auto& fields = premise.pre->node->GetType().fields;
    return plankton::All(fields, [&premise, &conclusion](const auto& field) {
        return !plankton::UpdatesField(conclusion, field.first) || plankton::UpdatesField(premise, field.first);
    });
}

inline void AddEffectImplicationCheck(Encoding& encoding, const HeapEffect& premise, const HeapEffect& conclusion,
                                      std::function<void()>&& eureka) {
    // give up if context contains resources
    if (!CheckContext(*premise.context) || !CheckContext(*conclusion.context)) return;
    
    // ensure that the premise updates at least the fields updated by the conclusion
    if (!UpdateSubset(premise, conclusion)) return;
    
    // encode
    auto premisePre = encoding.Encode(*premise.pre) && encoding.Encode(*premise.context);
    auto premisePost = encoding.Encode(*premise.post) && encoding.Encode(*premise.context);
    auto conclusionPre = encoding.Encode(*conclusion.pre) && encoding.Encode(*conclusion.context);
    auto conclusionPost = encoding.Encode(*conclusion.post) && encoding.Encode(*conclusion.context);
    auto samePre = encoding.EncodeMemoryEquality(*premise.pre, *conclusion.pre);
    auto samePost = encoding.EncodeMemoryEquality(*premise.post, *conclusion.post);

    // check
    auto isImplied = ((samePre && samePost && conclusionPre) >> premisePre)
                         && ((samePre && samePost && conclusionPost) >> premisePost); // TODO: correct?
    encoding.AddCheck(isImplied, [eureka=std::move(eureka)](bool holds) { if (holds) eureka(); });
}


inline EffectPairDeque ComputeEffectImplications(const EffectPairDeque& effectPairs) {
    EffectPairDeque result;
    Encoding encoding;
    for (const auto& pair : effectPairs) {
        auto eureka = [&result, pair]() { result.push_back(pair); };
        AddEffectImplicationCheck(encoding, *pair.first, *pair.second, std::move(eureka));
    }
    encoding.Check();
    return result;
}


//
// Adding new interference
//

inline void RenameEffect(HeapEffect& effect, SymbolFactory& factory) {
    auto renaming = plankton::MakeDefaultRenaming(factory);
    plankton::RenameSymbols(*effect.pre, renaming);
    plankton::RenameSymbols(*effect.post, renaming);
    plankton::RenameSymbols(*effect.context, renaming);
}

inline bool IsEffectEmpty(const HeapEffect& effect) {
    assert(effect.pre->node->GetType() == effect.post->node->GetType());
    if (effect.pre->flow->Decl() != effect.post->flow->Decl()) return false;
    for (const auto& [field, value] : effect.pre->fieldToValue) {
        if (value->Decl() != effect.post->fieldToValue.at(field)->Decl()) return false;
    }
    return true;
}

inline bool AreEffectsEqual(const HeapEffect& effect, const HeapEffect& other) {
    return plankton::SyntacticalEqual(*effect.pre, *other.pre)
           && plankton::SyntacticalEqual(*effect.post, *other.post)
           && plankton::SyntacticalEqual(*effect.context, *other.context);
}

inline void QuickFilter(std::deque<std::unique_ptr<HeapEffect>>& effects) {
    for (auto& effect : effects) {
        SymbolFactory factory;
        RenameEffect(*effect, factory);
    }

    for (auto& effect : effects) {
        if (!IsEffectEmpty(*effect)) continue;
        effect.reset(nullptr);
    }
    for (auto it = effects.begin(); it != effects.end(); ++it) {
        if (!*it) continue;
        for (auto ot = std::next(it); ot != effects.end(); ++ot) {
            if (!*ot) continue;
            if (!AreEffectsEqual(**it, **ot)) continue;
            ot->reset(nullptr);
        }
    }

    plankton::RemoveIf(effects, [](const auto& elem) { return !elem; });
}

inline void RenameEffects(std::deque<std::unique_ptr<HeapEffect>>& effects, const std::deque<std::unique_ptr<HeapEffect>>& interference) {
    SymbolFactory factory;
    plankton::AvoidEffectSymbols(factory, interference);
    for (auto& effect : effects) RenameEffect(*effect, factory);
}

void ReplaceInterfererTid(std::deque<std::unique_ptr<HeapEffect>>& interference) {
    for (const auto& effect : interference) {
        auto axioms = plankton::CollectMutable<StackAxiom>(*effect->context);
        for (const auto& axiom: axioms) {
            if (dynamic_cast<const SymbolicSelfTid*>(axiom->lhs.get())) {
                axiom->lhs = std::make_unique<SymbolicSomeTid>();
            }
            if (dynamic_cast<const SymbolicSelfTid*>(axiom->rhs.get())) {
                axiom->rhs = std::make_unique<SymbolicSomeTid>();
            }
        }
    }
}

bool Solver::AddInterference(std::deque<std::unique_ptr<HeapEffect>> effects) {
    DEBUG("Solver::AddInterference (" << effects.size() << ")" << std::endl)
    MEASURE("Solver::AddInterference")

    // preprocess
    ReplaceInterfererTid(effects);
    QuickFilter(effects);
    DEBUG("Number of effects after filter: " << effects.size() << std::endl)
    if (effects.empty()) return false;
    RenameEffects(effects, interference);
    
    // generate all pairs of effects and check implication, order matters
    EffectPairDeque pairs;
    for (auto& e : interference) for (auto& o : effects) pairs.emplace_back(e.get(), o.get());
    for (auto& e : effects) for (auto& o : interference) pairs.emplace_back(e.get(), o.get());
    for (auto& e : effects) for (auto& o : effects) if (e != o) pairs.emplace_back(e.get(), o.get());
    auto implications = ComputeEffectImplications(pairs);
    
    // find unnecessary effects
    std::set<const HeapEffect*> prune;
    for (const auto& [premise, conclusion] : implications) {
        if (prune.count(premise) != 0) continue;
        prune.insert(conclusion);
    }
    
    // remove unnecessary effects
    auto removePrunedEffects = [&prune](const auto& elem){ return plankton::Membership(prune, elem.get()); };
    plankton::RemoveIf(interference, removePrunedEffects);
    plankton::RemoveIf(effects, removePrunedEffects);
    
    // check if new effects exist
    if (effects.empty()) return false;
    
    INFO(std::endl << "Adding effects to solver (" << effects.size() << "): " << std::endl)
    for (const auto& effect : effects) INFO("   " << *effect << std::endl)
    INFO(std::endl)

    // add new effects
    plankton::MoveInto(std::move(effects), interference);
    return true;
}
