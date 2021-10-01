#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/util.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;


inline bool QuickMismatchCheck(const Annotation& premise, const Annotation& conclusion) {
    std::map<Specification, std::size_t> preSpec, conSpec;
    for (const auto* obl : plankton::Collect<ObligationAxiom>(*premise.now)) preSpec[obl->spec]++;
    for (const auto* obl : plankton::Collect<ObligationAxiom>(*conclusion.now)) conSpec[obl->spec]++;

    for (const auto& [spec, count] : conSpec) {
        if (preSpec[spec] < count) return true;
    }

    return plankton::Collect<EqualsToAxiom>(*premise.now).size()
           != plankton::Collect<EqualsToAxiom>(*conclusion.now).size();
}

inline bool NowSyntacticallyIncluded(const SeparatingConjunction& premise, const SeparatingConjunction& conclusion) {
    assert(plankton::Collect<EqualsToAxiom>(premise).size() == plankton::Collect<EqualsToAxiom>(conclusion).size());
    std::set<const Formula*> missing;
    for (const auto& conjunct : conclusion.conjuncts) {
        auto subsumed = plankton::Any(premise.conjuncts, [&conjunct](const auto& other){
            return plankton::SyntacticalEqual(*conjunct, *other);
        });
        if (!subsumed) return false;
    }
    return true;
}

inline bool PastSyntacticallyIncluded(const Annotation& premise, const Annotation& conclusion) {
    for (const auto& past : conclusion.past) {
        auto subsumed = plankton::Any(premise.past, [&past](const auto& other){
            return plankton::SyntacticalEqual(*past, *other);
        });
        if (!subsumed) return false;
    }
    return true;
}

inline bool FutureSyntacticallyIncluded(const Annotation& /*premise*/, const Annotation& conclusion) {
    if (conclusion.future.empty()) return true;
    throw std::logic_error("Not yet implemented: syntactic inclusion among futures.");
}

inline bool SyntacticallyIncluded(const Annotation& premise, const Annotation& conclusion) {
    return NowSyntacticallyIncluded(*premise.now, *conclusion.now)
           && PastSyntacticallyIncluded(premise, conclusion)
           && FutureSyntacticallyIncluded(premise, conclusion);
}

struct AxiomAnalyser : public DefaultLogicVisitor {
    bool result = false;

    bool IsStack(const LogicObject& object) {
        result = false;
        object.Accept(*this);
        return result;
    }

    void Visit(const StackAxiom& /*object*/) override { result = true; }
    void Visit(const InflowEmptinessAxiom& /*object*/) override { result = true; }
    void Visit(const InflowContainsValueAxiom& /*object*/) override { result = true; }
    void Visit(const InflowContainsRangeAxiom& /*object*/) override { result = true; }
};

inline bool IsStack(const std::unique_ptr<Formula>& object) {
    static AxiomAnalyser analyser;
    return analyser.IsStack(*object);
}

inline std::unique_ptr<Annotation> Strip(const Annotation& annotation) {
    auto result = plankton::Copy(annotation);
    plankton::RemoveIf(result->now->conjuncts, IsStack);
    return result;
}

inline bool ResourcesMatch(const Annotation& premise, const Annotation& conclusion) {
    return SyntacticallyIncluded(*Strip(premise), *Strip(conclusion));
}

inline bool StackImplies(const Annotation& premise, const SeparatingConjunction& conclusion, const SolverConfig& config) {
    MEASURE("Solver::Implies ~> StackImplies")
    Encoding encoding(*premise.now);
    encoding.AddPremise(encoding.EncodeInvariants(*premise.now, config));
    for (const auto& past : premise.past) encoding.AddPremise(encoding.EncodeInvariants(*past->formula, config));
    return encoding.Implies(conclusion);
}

inline void TryAvoidResourceMismatch(Annotation& premise, Annotation& conclusion, const SolverConfig& config) {
    // TODO: extend stack with pointer equalities?
    std::set<const SymbolDeclaration*> memories;
    for (auto* elem : plankton::Collect<SharedMemoryCore>(*premise.now)) memories.insert(&elem->node->Decl());
    for (auto* elem : plankton::Collect<SharedMemoryCore>(*conclusion.now)) memories.insert(&elem->node->Decl());
    plankton::MakeMemoryAccessible(premise, memories, config);
    // plankton::MakeMemoryAccessible(conclusion, memories, config);
}

bool Solver::Implies(const Annotation& premise, const Annotation& conclusion) const {
    MEASURE("Solver::Implies")
    if (QuickMismatchCheck(premise, conclusion)) return false;
    // DEBUG("== CHK IMP " << premise << " ==> " << conclusion << std::endl)

    auto normalizedPremise = plankton::Normalize(plankton::Copy(premise));
    auto normalizedConclusion = plankton::Normalize(plankton::Copy(conclusion));
    if (SyntacticallyIncluded(*normalizedPremise, *normalizedConclusion)) return true;
    // DEBUG("== CHK IMP deep " << *normalizedPremise << " ==> " << *normalizedConclusion << std::endl)
    
    TryAvoidResourceMismatch(*normalizedPremise, *normalizedConclusion, config);
    normalizedPremise = plankton::Normalize(std::move(normalizedPremise));
    normalizedConclusion = plankton::Normalize(std::move(normalizedConclusion));

    if (SyntacticallyIncluded(*normalizedPremise, *normalizedConclusion)) return true;
    DEBUG("== CHK IMP sem " << *normalizedPremise << " ==> " << *normalizedConclusion << std::endl)
    return ResourcesMatch(*normalizedPremise, *normalizedConclusion) &&
           StackImplies(*normalizedPremise, *normalizedConclusion->now, config);
}
