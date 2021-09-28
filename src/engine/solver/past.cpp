#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;

/* TODO:
 *   1) solve memory expansion immediately, then filter before interpolating
 *   2) solve more candidates, if it does not affect runtime
 *   3) faster filter
 */



inline SymbolRenaming MakeRenaming(const MemoryAxiom& replace, const MemoryAxiom& with) {
    assert(replace.node->Type() == with.node->Type());
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> map;
    map[&replace.node->Decl()] = &with.node->Decl();
    map[&replace.flow->Decl()] = &with.flow->Decl();
    for (const auto& [name, value] : replace.fieldToValue) {
        map[&value->Decl()] = &with.fieldToValue.at(name)->Decl();
    }

    return [map=std::move(map)](const SymbolDeclaration& decl) -> const SymbolDeclaration& {
        auto find = map.find(&decl);
        if (find != map.end()) return *find->second;
        else return decl;
    };
}

//
// Filter
//

inline Encoding MakeEncoding(const Annotation& annotation, const SolverConfig& config) {
    Encoding encoding(*annotation.now);
    encoding.AddPremise(encoding.EncodeInvariants(*annotation.now, config));
    for (const auto& elem : annotation.past)
        encoding.AddPremise(encoding.EncodeInvariants(*elem->formula, config));
    return encoding;
}

// inline std::optional<EExpr>
// MakeCheck(Encoding& encoding, const Formula& formula, const MemoryAxiom& weaker, const MemoryAxiom& stronger) {
//     if (&stronger == &weaker) return std::nullopt;
//     if (weaker.node->Decl() != stronger.node->Decl()) return std::nullopt;
//     auto result = plankton::MakeVector<EExpr>(weaker.fieldToValue.size() + 2);
//
//     auto addEq = [&result,&encoding](const auto& decl, const auto& other) {
//         result.push_back(encoding.Encode(decl) == encoding.Encode(other));
//     };
//     for (const auto* memory : plankton::Collect<MemoryAxiom>(formula)) {
//         if (memory->node->Type() != weaker.node->Type()) continue;
//         if (weaker.flow->Decl() == memory->flow->Decl()) addEq(memory->flow->Decl(), stronger.flow->Decl());
//         for (const auto& [name, value] : weaker.fieldToValue) {
//             if (value->Decl() != memory->fieldToValue.at(name)->Decl()) continue;
//             addEq(value->Decl(), stronger.fieldToValue.at(name)->Decl());
//         }
//     }
//
//     auto renaming = MakeRenaming(weaker, stronger);
//     auto renamed = plankton::Copy(formula);
//     plankton::RenameSymbols(*renamed, renaming);
//     result.push_back(encoding.Encode(*renamed));
//
//     return encoding.MakeAnd(result);
// }

inline std::optional<EExpr>
MakeCheck(Encoding& encoding, const Formula& formula, const MemoryAxiom& weaker, const MemoryAxiom& stronger) {
    if (&stronger == &weaker) return std::nullopt;
    if (weaker.node->Decl() != stronger.node->Decl()) return std::nullopt;
    auto renaming = MakeRenaming(weaker, stronger);
    auto renamed = plankton::Copy(formula);
    plankton::RenameSymbols(*renamed, renaming);
    return encoding.Encode(*renamed);
}

void FilterPasts(Annotation& annotation, const SolverConfig& config) {
    Timer timer("FilterPasts"); auto measure = timer.Measure();
    if (annotation.past.empty()) return;

//    plankton::RemoveIf(annotation.past,[&annotation](const auto& elem){
//        return !plankton::TryGetResource(elem->formula->node->Decl(), *annotation.now);
//    });
//    if (annotation.past.empty()) return;

    plankton::InlineAndSimplify(annotation);
    auto encoding = MakeEncoding(annotation, config);
    // DEBUG("Filtering past " << annotation << std::endl)

    // TODO: remove pasts that are weaker than now
    std::set<std::pair<const LogicObject*, const LogicObject*>> subsumption;
    for (const auto& stronger : annotation.past) {
        for (const auto& weaker : annotation.past) {
            auto check = MakeCheck(encoding, *annotation.now, *weaker->formula, *stronger->formula);
            if (!check) continue;

            // if (encoding.Implies(*check)) {
            //     DEBUG("past imp: " << *stronger << "  ==>  " << *weaker << std::endl)
            //     subsumption.emplace(weaker.get(), stronger.get());
            // }
            encoding.AddCheck(*check, [&subsumption, &weaker, &stronger](bool holds) {
                if (holds) subsumption.emplace(weaker.get(), stronger.get());
            });
        }
    }
    DEBUG("  starting to solve filter..." << std::flush)
    encoding.Check();
    DEBUG(" done" << std::endl)

    std::set<const LogicObject*> prune;
    std::set<std::pair<const LogicObject*, const LogicObject*>> blacklist;
    for (const auto& [weaker, stronger] : subsumption) {
        if (plankton::Membership(blacklist, std::make_pair(stronger, weaker))) continue; // break circles
        blacklist.emplace(weaker, stronger);
        prune.insert(weaker);
    }
    plankton::RemoveIf(annotation.past, [&prune](const auto& elem) {
        return plankton::Membership(prune, elem.get());
    });

    // DEBUG("FILTERING past gave " << annotation << std::endl)
}

void Solver::PrunePast(Annotation& annotation) const {
    // TODO: should this be available to ImprovePast only?
    FilterPasts(annotation, config);
}



//
// Interpolate
//

// constexpr const ExtensionPolicy POLICY = ExtensionPolicy::FAST;
constexpr const std::string_view DUMMY_FLOW_FIELD = "__@flow#__"; // TODO: ensure this name is not used
constexpr const bool INTERPOLATE_POINTERS_ONLY = true;

inline std::unique_ptr<StackAxiom> MakeEq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
    return std::make_unique<StackAxiom>(
            BinaryOperator::EQ, std::make_unique<SymbolicVariable>(decl), std::make_unique<SymbolicVariable>(other)
    );
}

inline std::set<const SymbolDeclaration*> GetActiveReferences(const Annotation& annotation) {
    std::set<const SymbolDeclaration*> result;
    for (auto variable : plankton::Collect<EqualsToAxiom>(*annotation.now)) {
        result.insert(&variable->Value());
    }
    return result;
}

inline const SymbolDeclaration& GetFieldValue(const SharedMemoryCore& memory, const std::string& field) {
    if (field == DUMMY_FLOW_FIELD) return memory.flow->Decl();
    else return memory.fieldToValue.at(field)->Decl();
}

using ImmutabilityLookup = std::map<std::pair<const SymbolDeclaration*, std::string>, const SymbolDeclaration*>;

inline ImmutabilityLookup
MakeImmutabilityLookup(const Annotation& annotation, const std::deque<std::unique_ptr<HeapEffect>>& interference) {
    std::map<std::pair<const SymbolDeclaration*, std::string>, const SymbolDeclaration*> result;
    auto memories = plankton::Collect<SharedMemoryCore>(*annotation.now);
    for (const auto* memory : memories) {
        // if (plankton::All(interference, [](const auto& elem){
        //     return elem->pre->flow->Decl() == elem->post->flow->Decl();
        // })) result[{&memory->node->Decl(), std::string(DUMMY_FLOW_FIELD)}] = &memory->flow->Decl();

        for (const auto& [field, value] : memory->fieldToValue) {
            if (plankton::Any(interference, [field=field](const auto& elem){
                return elem->pre->fieldToValue.at(field)->Decl() != elem->post->fieldToValue.at(field)->Decl();
            })) continue;
            result[{&memory->node->Decl(), field}] = &value->Decl();
        }
    }
    return result;
}

struct InterpolationInfo {
    EExpr premise;
    std::unique_ptr<SharedMemoryCore> past;
    std::deque<std::unique_ptr<Axiom>> candidates;
};

struct Interpolator {
    const std::deque<std::unique_ptr<HeapEffect>>& interference;
    const SolverConfig& config;
    Annotation& annotation;
    SymbolFactory factory;
    Encoding encoding;
    std::deque<InterpolationInfo> preparation;
    ImmutabilityLookup immutability;

    Interpolator(Annotation& annotation, const std::deque<std::unique_ptr<HeapEffect>>& interference,
                 const SolverConfig& config) : interference(interference), config(config), annotation(annotation) {
        plankton::Simplify(annotation);
        plankton::AvoidEffectSymbols(factory, interference);
        plankton::RenameSymbols(annotation, factory);
        encoding = MakeEncoding(annotation, config);
        immutability = MakeImmutabilityLookup(annotation, interference);
    }

    void Interpolate() {
        ApplyImmutability();
        FilterPasts(annotation, config);
        // CopyNowMemory();
        ExpandHistoryMemory();
        PreparePastToNowInterpolation();
        ApplyImmutability();
        SolveInterpolation();
        FilterPasts(annotation, config);
        PostProcess();
    }

    void ApplyImmutability(MemoryAxiom& memory) {
        for (auto& [field, value] : memory.fieldToValue) {
            auto newValue = immutability[{ &memory.node->Decl(), field }];
            if (!newValue) continue;
            annotation.Conjoin(MakeEq(*newValue, value->Decl()));
            encoding.AddPremise(encoding.Encode(*value) == encoding.Encode(*newValue));
            value->decl = *newValue;
        }
    }

    void ApplyImmutability() {
        for (auto& elem : annotation.past) ApplyImmutability(*elem->formula);
        for (auto& elem : preparation) ApplyImmutability(*elem.past);
    }

    inline void AddNewPast(const EExpr& premise, std::unique_ptr<SharedMemoryCore> past,
                           std::deque<std::unique_ptr<Axiom>> candidates = {}) {
        assert(past);
        encoding.AddPremise(encoding.EncodeInvariants(*past, config));
        // candidates = plankton::MakeStackCandidates(*annotation.now, *past, ExtensionPolicy::FAST); // TODO: activate as it does not seem to make a difference in runtime!?
        plankton::MoveInto(MakeCandidates(*past), candidates);
        preparation.push_back({ premise, std::move(past), std::move(candidates) });
    }

    [[nodiscard]] inline std::set<const SymbolDeclaration*> GetPointerFields(const SharedMemoryCore& memory) const {
        return plankton::Collect<SymbolDeclaration>(memory, [this](auto& obj){
            return obj.type.sort == Sort::PTR && plankton::TryGetResource(obj, *annotation.now);
        });
    }

    [[nodiscard]] inline std::unique_ptr<Annotation> MakeStackBlueprint() const {
        auto result = std::make_unique<Annotation>(plankton::Copy(*annotation.now));
        plankton::RemoveIf(result->now->conjuncts, [](const auto& conjunct){
            return dynamic_cast<const SharedMemoryCore*>(conjunct.get());
        });
        return result;
    }

    void ExpandHistoryMemory() {
        // static Timer timer("ExpandHistoryMemory"); auto measure = timer.Measure();
        // DEBUG("== ExpandHistoryMemory for " << annotation << std::endl)

        auto& flowType = config.GetFlowValueType();
        auto pasts = std::move(annotation.past);
        annotation.past.clear();

        for (auto& elem : pasts) {
            auto ptrFields = GetPointerFields(*elem->formula);
            auto expansion = std::make_unique<SeparatingConjunction>();
            expansion->Conjoin(std::move(elem->formula));
            {
                static Timer timer("ExpandHistoryMemory"); auto measure = timer.Measure();
                plankton::MakeMemoryAccessible(*expansion, ptrFields, flowType, factory, encoding);
            }

            for (const auto& conjunct : expansion->conjuncts) {
                if (auto memory = dynamic_cast<SharedMemoryCore*>(conjunct.get())) {
                    annotation.Conjoin(std::make_unique<PastPredicate>(plankton::Copy(*memory)));
                    auto candidates = plankton::MakeStackCandidates(*annotation.now, *memory, ExtensionPolicy::FAST);
                    AddNewPast(encoding.Bool(true), plankton::Copy(*memory), std::move(candidates));
                } else {
                    // encoding.AddPremise(*conjunct);
                    // annotation.Conjoin(std::move(conjunct));
                }
            }

            auto past = MakeStackBlueprint();
            plankton::MoveInto(std::move(expansion->conjuncts), past->now->conjuncts);
            encoding.AddPremise(encoding.EncodeInvariants(*past->now, config));
            encoding.AddPremise(encoding.EncodeSimpleFlowRules(*past->now, config));
        }
    }

    [[nodiscard]] inline std::deque<std::unique_ptr<Axiom>> MakeCandidates(const SharedMemoryCore& memory) const {
        std::deque<std::unique_ptr<Axiom>> result;
        for (const auto& effect : interference) {
            auto axioms = plankton::Collect<Axiom>(*effect->context);
            auto preRenaming = MakeRenaming(*effect->pre, memory);
            auto postRenaming = MakeRenaming(*effect->post, memory);

            for (const auto* axiom : axioms) {
                auto candidate = plankton::Copy(*axiom);
                plankton::RenameSymbols(*candidate, preRenaming);
                plankton::RenameSymbols(*candidate, postRenaming);
                result.push_back(std::move(candidate));
            }
        }
        return result;
    }

    inline void PreparePastToNowFieldInterpolation(const SharedMemoryCore& now, const SharedMemoryCore& past,
                                                   const std::string& field) {
        auto& nowValue = GetFieldValue(now, field);
        auto& pastValue = GetFieldValue(past, field);
        if (nowValue == pastValue) return;
        if (INTERPOLATE_POINTERS_ONLY && nowValue.type.sort != Sort::PTR) return;
        auto updatesField = [&field](const auto& effect) {
            if (field == DUMMY_FLOW_FIELD) return plankton::UpdatesFlow(effect);
            else return plankton::UpdatesField(effect, field);
        };

        auto newHistory = plankton::MakeSharedMemory(now.node->Decl(), config.GetFlowValueType(), factory);
        if (field == DUMMY_FLOW_FIELD) newHistory->flow->decl = nowValue;
        else newHistory->fieldToValue.at(field)->decl = nowValue;

        auto vector = plankton::MakeVector<EExpr>(interference.size() + 1);
        vector.push_back(
                // 'field' was never changed
                encoding.Encode(nowValue) == encoding.Encode(pastValue) &&
                encoding.EncodeMemoryEquality(past, *newHistory)
        );
        for (const auto& effect : interference) {
            if (!updatesField(*effect)) continue;
            auto& effectValue = GetFieldValue(*effect->post, field);
            vector.push_back(
                    // last update to 'field' is due to 'effect'
                    // encoding.Encode(nowValue) != encoding.Encode(pastValue) &&
                    encoding.Encode(nowValue) == encoding.Encode(effectValue) &&
                    encoding.Encode(*effect->context) &&
                    encoding.EncodeMemoryEquality(*effect->post, *newHistory)
            );
        }

        // if (vector.size() <= 1) {
        //     newHistory = plankton::Copy(past);
        //     if (field == DUMMY_FLOW_FIELD) newHistory->flow->decl = nowValue;
        //     else newHistory->fieldToValue.at(field)->decl = nowValue;
        //     annotation.Conjoin(std::make_unique<PastPredicate>(std::move(newHistory)));
        // }

        AddNewPast(encoding.MakeOr(vector), std::move(newHistory));
    }

    void PreparePastToNowInterpolation() {
        auto referenced = GetActiveReferences(annotation);
        for (const auto* nowMem : plankton::Collect<SharedMemoryCore>(*annotation.now)) {
            if (!plankton::Membership(referenced, &nowMem->node->Decl())) continue;
            for (const auto& past : annotation.past) {
                if (nowMem->node->Decl() != past->formula->node->Decl()) continue;
                PreparePastToNowFieldInterpolation(*nowMem, *past->formula, std::string(DUMMY_FLOW_FIELD));
                for (const auto& pair : nowMem->fieldToValue) {
                    PreparePastToNowFieldInterpolation(*nowMem, *past->formula, pair.first);
                }
            }
        }
    }

    void SolveInterpolation() {
        ApplyImmutability();
        annotation.past.clear();

        std::size_t counter = 0;
        for (auto& [premise, memory, candidates] : preparation) {
            annotation.Conjoin(std::make_unique<PastPredicate>(std::move(memory)));

            for (auto& candidate : candidates) {
                auto encoded = encoding.Encode(*candidate);
                encoding.AddCheck(premise >> encoded, [this,&candidate](bool holds) {
                    if (holds) annotation.Conjoin(std::move(candidate));
                });
                counter++;
            }
        }

        DEBUG("  starting to solve interpolation #counter=" << counter << "..." << std::flush)
        {
            static Timer timer("SolveInterpolation"); auto measure = timer.Measure();
            encoding.Check();
        }
        DEBUG(" done" << std::endl)
    }

    void PostProcess() {
        plankton::InlineAndSimplify(annotation);
    }
};

void Solver::ImprovePast(Annotation& annotation) const {
    static Timer timer("ImprovePast"); auto measure = timer.Measure();
    if (annotation.past.empty()) return;
    Interpolator(annotation, interference, config).Interpolate();
}
