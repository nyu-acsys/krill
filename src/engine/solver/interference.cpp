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
    const auto& fields = premise.pre->node->Type().fields;
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

inline bool IsEffectEmpty(const std::unique_ptr<HeapEffect>& effect) {
    assert(effect->pre->node->Type() == effect->post->node->Type());
    if (effect->pre->flow->Decl() != effect->post->flow->Decl()) return false;
    for (const auto& [field, value] : effect->pre->fieldToValue) {
        if (value->Decl() != effect->post->fieldToValue.at(field)->Decl()) return false;
    }
    return true;
}

inline void RenameEffects(std::deque<std::unique_ptr<HeapEffect>>& effects,
                          const std::deque<std::unique_ptr<HeapEffect>>& interference) {
    SymbolFactory factory;
    plankton::AvoidEffectSymbols(factory, interference);
    
    for (auto& effect : effects) {
        auto renaming = plankton::MakeDefaultRenaming(factory);
        plankton::RenameSymbols(*effect->pre, renaming);
        plankton::RenameSymbols(*effect->post, renaming);
        plankton::RenameSymbols(*effect->context, renaming);
    }
}

bool Solver::AddInterference(std::deque<std::unique_ptr<HeapEffect>> effects) {
    MEASURE("Solver::AddInterference")

    // prune trivial noop interference
    plankton::RemoveIf(effects, IsEffectEmpty);
    if (effects.empty()) return false;
    
    // rename to avoid name clashes
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
    
    DEBUG(std::endl << "New effects: " << std::endl)
    for (const auto& effect : effects) DEBUG("** effect: " << *effect << std::endl)
    
    // add new effects
    plankton::MoveInto(std::move(effects), interference);
    return true;
}
