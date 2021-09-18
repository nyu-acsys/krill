#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;


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

inline std::optional<EExpr>
MakeCheck(Encoding& encoding, const Formula& formula, const MemoryAxiom& weaker, const MemoryAxiom& stronger) {
    if (&stronger == &weaker) return std::nullopt;
    if (weaker.node->Decl() != stronger.node->Decl()) return std::nullopt;
    auto result = plankton::MakeVector<EExpr>(weaker.fieldToValue.size() + 2);
    
    auto addEq = [&result,&encoding](const auto& decl, const auto& other) {
        result.push_back(encoding.Encode(decl) == encoding.Encode(other));
    };
    for (const auto* memory : plankton::Collect<MemoryAxiom>(formula)) {
        if (memory->node->Type() != weaker.node->Type()) continue;
        if (weaker.flow->Decl() == memory->flow->Decl()) addEq(memory->flow->Decl(), stronger.flow->Decl());
        for (const auto& [name, value] : weaker.fieldToValue) {
            if (value->Decl() != memory->fieldToValue.at(name)->Decl()) continue;
            addEq(value->Decl(), stronger.fieldToValue.at(name)->Decl());
        }
    }
    
    auto renaming = MakeRenaming(weaker, stronger);
    auto renamed = plankton::Copy(formula);
    plankton::RenameSymbols(*renamed, renaming);
    result.push_back(encoding.Encode(*renamed));
    
    return encoding.MakeAnd(result);
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
    DEBUG("Filtering past " << annotation << std::endl)
    
    // TODO: remove pasts that are weaker than now
    std::set<std::pair<const LogicObject*, const LogicObject*>> subsumption;
    for (const auto& stronger : annotation.past) {
        for (const auto& weaker : annotation.past) {
            auto check = MakeCheck(encoding, *annotation.now, *weaker->formula, *stronger->formula);
            if (!check) continue;
            
            if (encoding.Implies(*check)) {
                DEBUG("past imp: " << *stronger << "  ==>  " << *weaker << std::endl)
                subsumption.emplace(weaker.get(), stronger.get());
            }
//            encoding.AddCheck(*check, [&subsumption, &weaker, &stronger](bool holds) {
//                if (holds) subsumption.emplace(weaker.get(), stronger.get());
//            });
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
    
    DEBUG("FILTERING past gave " << annotation << std::endl)
}


//
// Interpolate
//

constexpr std::string_view DUMMY_FLOW_FIELD = "__@flow#__"; // TODO: ensure this name is not used

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

struct Interpolator {
    explicit Interpolator(Annotation& annotation, const std::deque<std::unique_ptr<HeapEffect>>& interference,
                          const SolverConfig& config)
            : interference(interference), config(config), annotation(annotation) {
        plankton::AvoidEffectSymbols(factory, interference);
        plankton::RenameSymbols(annotation, factory);
        encoding.AddPremise(*annotation.now);
        encoding.AddPremise(encoding.EncodeInvariants(*annotation.now, config));
    }
    
    void Interpolate() {
        DEBUG("=== Past interpolation for " << annotation << std::endl)
//        FilterPasts(annotation, config); // TODO: needed?
//        DEBUG("  [filtered 1/3] now extending past..." << std::endl)
        ExpandPastMemory();
//        DEBUG("  [past memory extended] now filtering..." << std::endl)
//        FilterPasts(annotation, config); // TODO: needed?
//        DEBUG("  [filtered 2/3] now preparing..." << std::endl)
        Prepare();
//        DEBUG("  [prepared] now interpolating..." << std::endl)
        AddInterpolation();
//        DEBUG("  [interpolated] now filtering..." << std::endl)
        FilterPasts(annotation, config);
        DEBUG("=== Past interpolation yields: " << annotation << std::endl)
        throw std::logic_error("--- point du break ---");
    }
    
private:
    static constexpr ExtensionPolicy POLICY = ExtensionPolicy::FAST;
    const std::deque<std::unique_ptr<HeapEffect>>& interference;
    const SolverConfig& config;
    Annotation& annotation;
    SymbolFactory factory;
    Encoding encoding;
    std::deque<std::pair<EExpr, std::unique_ptr<SharedMemoryCore>>> preparation;
    
    [[nodiscard]] std::set<const SymbolDeclaration*> GetPointerFields(const SharedMemoryCore& memory) const {
        return plankton::Collect<SymbolDeclaration>(memory, [this](auto& obj){
            return obj.type.sort == Sort::PTR && plankton::TryGetResource(obj, *annotation.now);
        });
    }
    
    void ExpandPastMemory() {
        auto& flowType = config.GetFlowValueType();
        auto nowSymbols = plankton::Collect<SymbolDeclaration>(*annotation.now);

        std::deque<std::unique_ptr<PastPredicate>> newPast;
        for (const auto& elem : annotation.past) {
            auto ptrFields = GetPointerFields(*elem->formula);
            auto past = std::make_unique<Annotation>();
            past->Conjoin(std::move(elem->formula));
            plankton::MakeMemoryAccessible(*past->now, ptrFields, flowType, factory, encoding);
            
            encoding.Push();
            encoding.AddPremise(*past->now);
            encoding.AddPremise(encoding.EncodeInvariants(*past->now, config));
            encoding.AddPremise(encoding.EncodeSimpleFlowRules(*past->now, config));;
            plankton::ExtendStack(*past, encoding, POLICY);
            encoding.Pop();
            
            // TODO: improve this
            for (auto& conjunct : past->now->conjuncts) {
                if (auto memory = dynamic_cast<const SharedMemoryCore*>(conjunct.get())) {
                    newPast.push_back(std::make_unique<PastPredicate>(plankton::Copy(*memory)));
                } else {
                    annotation.Conjoin(std::move(conjunct));
                }
            }
        }
        
        annotation.past = std::move(newPast);
    }
    
    void PrepareField(const SharedMemoryCore& now, const SharedMemoryCore& past, const std::string& field) {
        auto& nowValue = GetFieldValue(now, field);
        auto& pastValue = GetFieldValue(past, field);
        if (nowValue == pastValue) return;

        auto newHistory = plankton::MakeSharedMemory(now.node->Decl(), config.GetFlowValueType(), factory);
        auto& histValue = GetFieldValue(*newHistory, field);
        auto updatesField = [&field](const auto& effect) {
            if (field == DUMMY_FLOW_FIELD) return plankton::UpdatesFlow(effect);
            else return plankton::UpdatesField(effect, field);
        };

        auto vector = plankton::MakeVector<EExpr>(interference.size() + 1);
        vector.push_back(
                // 'field' was never changed
                encoding.Encode(nowValue) == encoding.Encode(pastValue) &&
                encoding.Encode(pastValue) == encoding.Encode(histValue) &&
                encoding.EncodeMemoryEquality(past, *newHistory)
        );
        for (const auto& effect : interference) {
            if (!updatesField(*effect)) continue;
            auto& effectValue = GetFieldValue(*effect->post, field);
            vector.push_back(
                    // last update to 'field' is due to 'effect'
                    encoding.Encode(nowValue) != encoding.Encode(pastValue) &&
                    encoding.Encode(nowValue) == encoding.Encode(histValue) &&
                    encoding.Encode(effectValue) == encoding.Encode(histValue) &&
                    encoding.Encode(*effect->context) &&
                    encoding.EncodeMemoryEquality(*effect->post, *newHistory)
            );
        }

        preparation.emplace_back(encoding.MakeOr(vector), std::move(newHistory));
    }

    void Prepare() {
        auto referenced = GetActiveReferences(annotation);
        for (const auto* nowMem : plankton::Collect<SharedMemoryCore>(*annotation.now)) {
            if (!plankton::Membership(referenced, &nowMem->node->Decl())) continue;
            for (const auto& past : annotation.past) {
                if (nowMem->node->Decl() != past->formula->node->Decl()) continue;
                PrepareField(*nowMem, *past->formula, std::string(DUMMY_FLOW_FIELD));
                for (const auto& pair : nowMem->fieldToValue) {
                    PrepareField(*nowMem, *past->formula, pair.first);
                }
            }
        }
    }
    
    void AddInterpolation() {
        Timer timer("AddInterpolation"); auto measure = timer.Measure();
        if (preparation.empty()) return;
//        DEBUG("== Semantic Interpolation for: " << annotation << std::endl)
        
        // add things that might have been added
        encoding.AddPremise(*annotation.now);
        // encoding.AddPremise(encoding.EncodeInvariants(*annotation.now, config));
        
        // move past predicates into annotation, remember their symbols
        auto premises = plankton::MakeVector<std::pair<EExpr, std::set<const SymbolDeclaration*>>>(preparation.size());
        for (auto& [premise, past] : preparation) {
            premises.emplace_back(premise, plankton::Collect<SymbolDeclaration>(*past));
            annotation.Conjoin(std::make_unique<PastPredicate>(std::move(past)));
        }
        
        // create and check candidates
        std::size_t counter = 0;
        auto candidates = plankton::MakeStackCandidates(annotation, POLICY);
        for (auto& candidate : candidates) {
            auto encoded = encoding.Encode(*candidate);
            auto symbols = plankton::Collect<SymbolDeclaration>(*candidate);
            
            auto check = plankton::MakeVector<EExpr>(premises.size());
            for (const auto& [premise, others] : premises) {
                if (plankton::EmptyIntersection(symbols, others)) continue;
                check.push_back(premise >> encoded);
            }
            if (check.empty()) continue;
            
            encoding.AddCheck(encoding.MakeOr(check), [this,&candidate](bool holds) {
                if (holds) annotation.Conjoin(std::move(candidate));
            });
            counter++;
        }
        DEBUG("  starting to solve interpolation #candidates=" << candidates.size() << " #counter=" << counter << "..." << std::flush)
        encoding.Check();
        DEBUG(" done" << std::endl)
//        DEBUG("== Semantic Interpolation gave: " << annotation << std::endl)
    }
};

void Solver::ImprovePast(Annotation& annotation) const {
    Timer timer("ImprovePast"); auto measure = timer.Measure();
    if (annotation.past.empty()) return;
    Interpolator(annotation, interference, config).Interpolate();
}

void Solver::PrunePast(Annotation& annotation) const {
    FilterPasts(annotation, config);
}
