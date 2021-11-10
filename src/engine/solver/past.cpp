#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/shortcuts.hpp"
#include "util/timer.hpp"
#include "util/log.hpp"

using namespace plankton;

constexpr const bool INTERPOLATE_POINTERS_ONLY = true;


inline Encoding MakeEncoding(const Annotation& annotation, const SolverConfig& config) {
    Encoding encoding(*annotation.now);
    encoding.AddPremise(encoding.EncodeInvariants(*annotation.now, config));
    // encoding.AddPremise(encoding.EncodeSimpleFlowRules(*annotation.now, config));
    for (const auto& elem : annotation.past)
        encoding.AddPremise(encoding.EncodeInvariants(*elem->formula, config));
    return encoding;
}


//
// Filter
//

inline std::optional<EExpr>
MakeCheck(Encoding& encoding, const Formula& formula, const MemoryAxiom& weaker, const MemoryAxiom& stronger) {
    if (&stronger == &weaker) return std::nullopt;
    if (weaker.node->Decl() != stronger.node->Decl()) return std::nullopt;
    auto renaming = plankton::MakeMemoryRenaming(weaker, stronger);
    auto renamed = plankton::Copy(formula);
    plankton::RenameSymbols(*renamed, renaming);
    return encoding.Encode(*renamed);
}

void FilterPasts(Annotation& annotation, const SolverConfig& config) {
    MEASURE("Solver::PrunePast ~> FilterPasts")
    if (annotation.past.empty()) return;

    // // TODO: is this clever? ~~> no it is not... :*(
    // plankton::RemoveIf(annotation.past,[&annotation](const auto& elem){
    //    return !plankton::TryGetResource(elem->formula->node->Decl(), *annotation.now);
    // });
    // if (annotation.past.empty()) return;

    plankton::InlineAndSimplify(annotation);
    auto encoding = MakeEncoding(annotation, config);

    std::map<const PastPredicate*, std::set<const PastPredicate*>> subsumes;
    for (const auto& stronger : annotation.past) {
        for (const auto& weaker : annotation.past) {
            auto check = MakeCheck(encoding, *annotation.now, *weaker->formula, *stronger->formula);
            if (!check) continue;
            encoding.AddCheck(*check, [&subsumes, &weaker, &stronger](bool holds) {
                // DEBUG("  past subsumption=" << holds << " for " << *stronger << " => " << *weaker << std::endl)
                if (holds) subsumes[stronger.get()].insert(weaker.get());
            });
        }
    }
    encoding.Check();

    for (const auto& past : annotation.past) {
        if (!past) continue;
        auto subsumption = subsumes[past.get()];
        if (subsumption.empty()) continue;
        for (auto& other : annotation.past) {
            if (!other) continue;
            if (past.get() == other.get()) continue;
            if (!plankton::Membership(subsumption, other.get())) continue;
            other.reset(nullptr);
        }
    }
    plankton::RemoveIf(annotation.past, [](const auto& elem){ return !elem; });
}

void Solver::ReducePast(Annotation& annotation) const {
    MEASURE("Solver::PrunePast")
    DEBUG("<<REDUCE PAST>>" << std::endl)
    DEBUG(annotation << std::endl)
    assert(!IsUnsatisfiable(annotation));
    FilterPasts(annotation, config);
    DEBUG(" ~> " << annotation << std::endl << std::endl)
    assert(!IsUnsatisfiable(annotation));
}

std::unique_ptr<Annotation> Solver::ReducePast(std::unique_ptr<Annotation> annotation) const {
    ReducePast(*annotation);
    // DEBUG(*annotation << std::endl << std::endl)
    return annotation;
}


//
// Interpolate
//

inline std::unique_ptr<StackAxiom> MakeEq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
    return std::make_unique<StackAxiom>(
            BinaryOperator::EQ, std::make_unique<SymbolicVariable>(decl), std::make_unique<SymbolicVariable>(other)
    );
}

inline std::set<const SymbolDeclaration*> GetActiveReferences(const Annotation& annotation) {
    std::set<const SymbolDeclaration*> result;
    for (const auto* variable : plankton::Collect<EqualsToAxiom>(*annotation.now)) {
        result.insert(&variable->Value());
    }
    return result;
}

using ImmutabilityLookup = std::map<std::pair<const SymbolDeclaration*, std::string>, const SymbolDeclaration*>;

inline ImmutabilityLookup
MakeImmutabilityLookup(const Annotation& annotation, const std::deque<std::unique_ptr<HeapEffect>>& interference) {
    std::map<std::pair<const SymbolDeclaration*, std::string>, const SymbolDeclaration*> result;
    auto memories = plankton::Collect<SharedMemoryCore>(*annotation.now);
    for (const auto* memory : memories) {
        for (const auto& [field, value] : memory->fieldToValue) {
            if (plankton::Any(interference, [field=field](const auto& elem){
                return elem->pre->fieldToValue.at(field)->Decl() != elem->post->fieldToValue.at(field)->Decl();
            })) continue;
            result[{&memory->node->Decl(), field}] = &value->Decl();
        }
    }
    return result;
}

struct Interpolator {
    const std::deque<std::unique_ptr<HeapEffect>>& interference;
    const SolverConfig& config;
    Annotation& annotation;
    SymbolFactory factory;
    std::optional<ImmutabilityLookup> immutability;

    Interpolator(Annotation& annotation, const std::deque<std::unique_ptr<HeapEffect>>& interference,
                 const SolverConfig& config) : interference(interference), config(config), annotation(annotation) {
        plankton::Simplify(annotation);
        plankton::AvoidEffectSymbols(factory, interference);
        plankton::RenameSymbols(annotation, factory);
        for (auto& elem : annotation.past) ApplyImmutability(*elem->formula, annotation.now.get());
    }

    void Interpolate() {
        Filter();
        // DEBUG(" (first filter) " << annotation << " ~~> ")
        ExpandHistoryMemory();
        // DEBUG(" (expansion) " << annotation << " ~~> ")
        Filter(); // TODO: to filter or not to filter?
        // DEBUG(" (second filter) " << annotation << " ~~> ")
        InterpolatePastToNow();
        // DEBUG(" (interpolation) " << annotation << " ~~> ")
        // Filter();
        AddTrivialPast();
        // DEBUG(" (trivial past) " << annotation << " ~~> ")
        PostProcess();
        // DEBUG(" (postprocessing) " << annotation << " ~~> ")
    }

    inline void ApplyImmutability(MemoryAxiom& memory, SeparatingConjunction* addEq = nullptr) {
        if (!immutability) immutability = MakeImmutabilityLookup(annotation, interference);
        for (auto& [field, value] : memory.fieldToValue) {
            assert(immutability);
            auto newValue = immutability.value()[{ &memory.node->Decl(), field }];
            if (!newValue) continue;
            if (addEq) addEq->Conjoin(MakeEq(*newValue, value->Decl()));
            value->decl = *newValue;
        }
    }

    void Filter() {
        FilterPasts(annotation, config);
        immutability = std::nullopt; // needs to be recomputed since FilterPasts may change symbols
    }

    inline void AddDerivedCandidates(Encoding& encoding, std::deque<std::unique_ptr<Axiom>> candidates) {
        for (auto& candidate : candidates) {
            encoding.AddCheck(encoding.Encode(*candidate), [this,&candidate](bool holds){
                if (holds) annotation.Conjoin(std::move(candidate));
            });
        }
        encoding.Check();
    }

    [[nodiscard]] inline std::unique_ptr<Annotation> MakeStackBlueprint() const {
        auto result = std::make_unique<Annotation>(plankton::Copy(*annotation.now));
        plankton::RemoveIf(result->now->conjuncts, [](const auto& conjunct){
            return dynamic_cast<const SharedMemoryCore*>(conjunct.get());
        });
        return result;
    }

    [[nodiscard]] inline std::set<const SymbolDeclaration*> GetPointerFields(const SharedMemoryCore& memory) const {
        return plankton::Collect<SymbolDeclaration>(memory, [this](auto& obj){
            return obj.type.sort == Sort::PTR && plankton::TryGetResource(obj, *annotation.now);
        });
    }

    void ExpandHistoryMemory() {
        MEASURE("Solver::ImprovePast ~> ExpandHistoryMemory")

        auto& flowType = config.GetFlowValueType();
        auto pasts = std::move(annotation.past);
        annotation.past.clear();

        auto referenced = GetActiveReferences(annotation);
        auto getExpansion = [this,&referenced](const auto& past) {
            auto ptrFields = GetPointerFields(*past.formula);
            for (auto it = ptrFields.begin(); it != ptrFields.end();) {
                if (plankton::Membership(referenced, *it)) ++it;
                else it = ptrFields.erase(it);
            }
            return ptrFields;
        };

        for (auto& elem : pasts) {
            auto& pastAddress = elem->formula->node->Decl();
            // if (!plankton::Membership(referenced, &pastAddress)) continue;

            auto expansion = getExpansion(*elem);
            auto past = MakeStackBlueprint();
            past->Conjoin(std::move(elem->formula));

            Encoding encoding = MakeEncoding(*past, config);
            plankton::MakeMemoryAccessible(*past->now, expansion, flowType, factory, encoding);

            std::deque<std::unique_ptr<Axiom>> candidates;
            for (auto memory : plankton::CollectMutable<SharedMemoryCore>(*past->now)) {
                ApplyImmutability(*memory, past->now.get());
                annotation.Conjoin(std::make_unique<PastPredicate>(plankton::Copy(*memory)));

                if (memory->node->Decl() == pastAddress) continue;
                auto memoryCandidates = plankton::MakeStackCandidates(*past->now, *memory, ExtensionPolicy::FAST);
                plankton::MoveInto(std::move(memoryCandidates), candidates);
            }

            encoding.AddPremise(*past->now);
            encoding.AddPremise(encoding.EncodeInvariants(*past->now, config));
            encoding.AddPremise(encoding.EncodeSimpleFlowRules(*past->now, config));
            // AddDerivedCandidates(encoding, plankton::MakeStackCandidates(*past, ExtensionPolicy::FAST));
            AddDerivedCandidates(encoding, std::move(candidates));
        }
    }

    [[nodiscard]] inline std::deque<std::unique_ptr<Axiom>> MakeInterpolationCandidates(const SharedMemoryCore& memory) const {
        std::deque<std::unique_ptr<Axiom>> result;
        for (const auto& effect : interference) {
            auto axioms = plankton::Collect<Axiom>(*effect->context);
            auto preRenaming = plankton::MakeMemoryRenaming(*effect->pre, memory);
            auto postRenaming = plankton::MakeMemoryRenaming(*effect->post, memory);

            for (const auto* axiom : axioms) {
                auto candidate = plankton::Copy(*axiom);
                plankton::RenameSymbols(*candidate, preRenaming);
                plankton::RenameSymbols(*candidate, postRenaming);
                result.push_back(std::move(candidate));
            }
        }
        return result;
    }

    inline void InterpolatePastToNow(const SharedMemoryCore& past, const std::string& field, const SymbolDeclaration& interpolatedValue) {
        auto& pastValue = past.fieldToValue.at(field)->Decl();
        if (interpolatedValue == pastValue) return;

        auto newHistory = plankton::MakeSharedMemory(past.node->Decl(), config.GetFlowValueType(), factory);
        ApplyImmutability(*newHistory);
        newHistory->fieldToValue.at(field)->decl = interpolatedValue;

        auto encoding = MakeEncoding(*MakeStackBlueprint(), config);
        encoding.AddPremise(encoding.EncodeInvariants(*newHistory, config));

        auto vector = plankton::MakeVector<EExpr>(interference.size() + 1);
        vector.push_back( // 'field' was never changed
                encoding.Encode(interpolatedValue) == encoding.Encode(pastValue) &&
                encoding.EncodeMemoryEquality(past, *newHistory)
        );
        for (const auto& effect : interference) {
            if (!plankton::UpdatesField(*effect, field)) continue;
            auto& effectValue = effect->post->fieldToValue.at(field)->Decl();
            vector.push_back( // last update to 'field' is due to 'effect'
                    encoding.Encode(interpolatedValue) == encoding.Encode(effectValue) &&
                    encoding.Encode(*effect->context) &&
                    encoding.EncodeMemoryEquality(*effect->post, *newHistory)
            );
        }
        encoding.AddPremise(encoding.MakeOr(vector));

        AddDerivedCandidates(encoding, MakeInterpolationCandidates(*newHistory));
        // AddDerivedCandidates(encoding, plankton::MakeStackCandidates(*annotation.now, *newHistory, ExtensionPolicy::FAST));
        annotation.Conjoin(std::make_unique<PastPredicate>(std::move(newHistory)));
    }

    //void InterpolatePastToNow() {
    //    MEASURE("Solver::ImprovePast ~> InterpolatePastToNow")
    //    auto referenced = GetActiveReferences(annotation);
    //    for (const auto* nowMem : plankton::Collect<SharedMemoryCore>(*annotation.now)) {
    //        if (!plankton::Membership(referenced, &nowMem->node->Decl())) continue;
    //        for (const auto& past : annotation.past) {
    //            if (nowMem->node->Decl() != past->formula->node->Decl()) continue;
    //            for (const auto& [field, value] : nowMem->fieldToValue) {
    //                if (INTERPOLATE_POINTERS_ONLY && value->Sort() != Sort::PTR) continue;
    //                InterpolatePastToNow(*past->formula, field, value->Decl());
    //            }
    //        }
    //    }
    //}

    void InterpolatePastToNow() {
        MEASURE("Solver::ImprovePast ~> InterpolatePastToNow")
        auto referenced = GetActiveReferences(annotation);
        auto nowMemories = plankton::Collect<SharedMemoryCore>(*annotation.now);
        for (const auto& past : annotation.past) {
            if (!plankton::Membership(referenced, &past->formula->node->Decl())) continue;
            std::set<const SymbolDeclaration*> interpolated;
            for (const auto* nowMem : nowMemories) {
                if (nowMem->node->Decl() != past->formula->node->Decl()) continue;
                for (const auto& [field, value] : nowMem->fieldToValue) {
                    if (INTERPOLATE_POINTERS_ONLY && value->Sort() != Sort::PTR) continue;
                    if (value->Sort() == Sort::PTR && !plankton::Membership(referenced, &value->Decl())) continue;
                    if (plankton::Membership(interpolated, &value->Decl())) continue;
                    InterpolatePastToNow(*past->formula, field, value->Decl());
                    interpolated.insert(&value->Decl());
                }
            }
        }
    }

    void AddTrivialPast() {
        for (const auto* memory : plankton::Collect<SharedMemoryCore>(*annotation.now)) {
            annotation.Conjoin(std::make_unique<PastPredicate>(plankton::Copy(*memory)));
        }
    }

    void PostProcess() {
        plankton::InlineAndSimplify(annotation);
    }

};

std::unique_ptr<Annotation> Solver::ImprovePast(std::unique_ptr<Annotation> annotation) const {
    MEASURE("Solver::ImprovePast")
    assert(!IsUnsatisfiable(*annotation));
    if (annotation->past.empty()) return annotation;
    DEBUG("<<IMPROVE PAST>>" << std::endl)
    DEBUG(*annotation << " ~~> ")
    Interpolator(*annotation, interference, config).Interpolate();
    DEBUG(*annotation << std::endl << std::endl)
    assert(!IsUnsatisfiable(*annotation));
    return annotation;
}
