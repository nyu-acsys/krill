#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;


constexpr std::string_view DUMMY_FLOW_FIELD = "__@flow#__"; // TODO: ensure this name is not used

inline const SymbolDeclaration& GetFieldValue(const SharedMemoryCore& memory, const std::string& field) {
    if (field == DUMMY_FLOW_FIELD) return memory.flow->Decl();
    else return memory.fieldToValue.at(field)->Decl();
}

struct Interpolator {
    explicit Interpolator(Annotation& annotation, const std::deque<std::unique_ptr<HeapEffect>>& interference,
                          const SolverConfig& config)
            : interference(interference), config(config), annotation(annotation) {
        AvoidEffectSymbols(factory, interference);
        plankton::RenameSymbols(annotation, factory);
        encoding.AddPremise(*annotation.now);
    }
    
    void Interpolate() {
        ExpandPastMemory();
        Filter();
        Prepare();
        AddInterpolation();
        Filter();
    }
    
private:
    static constexpr ExtensionPolicy POLICY = ExtensionPolicy::FAST;
    const std::deque<std::unique_ptr<HeapEffect>>& interference;
    const SolverConfig& config;
    Annotation& annotation;
    SymbolFactory factory;
    Encoding encoding;
    std::deque<std::pair<EExpr, std::unique_ptr<SharedMemoryCore>>> preparation;
    
    [[nodiscard]] static std::set<const SymbolDeclaration*> GetPointerFields(const SharedMemoryCore& memory) {
        return plankton::Collect<SymbolDeclaration>(memory, [](auto& obj){ return obj.type.sort == Sort::PTR; });
    }
    
    void ExpandPastMemory() {
        auto& flowType = config.GetFlowValueType();
        auto nowSymbols = plankton::Collect<SymbolDeclaration>(*annotation.now);
        plankton::MakeMemoryAccessible(*annotation.now, nowSymbols, flowType, factory, encoding);
        plankton::ExtendStack(annotation, encoding, POLICY);

        std::deque<std::unique_ptr<PastPredicate>> newPast;
        for (const auto& elem : annotation.past) {
            auto ptrFields = GetPointerFields(*elem->formula);
            auto past = std::make_unique<Annotation>();
            past->Conjoin(std::move(elem->formula));
            plankton::MakeMemoryAccessible(*past->now, ptrFields, flowType, factory, encoding);
            
            encoding.Push();
            auto graph = plankton::MakePureHeapGraph(std::move(past), factory, config);
            encoding.AddPremise(graph);
            plankton::ExtendStack(*graph.pre, encoding, POLICY);
            encoding.Push();
            
            // TODO: improve this
            for (auto& conjunct : graph.pre->now->conjuncts) {
                if (auto memory = dynamic_cast<const SharedMemoryCore*>(conjunct.get())) {
                    newPast.push_back(std::make_unique<PastPredicate>(plankton::Copy(*memory)));
                } else {
                    annotation.Conjoin(std::move(conjunct));
                }
            }
        }
        
        annotation.past = std::move(newPast);
    }
    
    void PrepareFieldCheck(const SharedMemoryCore& now, const SharedMemoryCore& past, const std::string& field) {
        auto updatesField = [&field](const auto& effect) {
            if (field == DUMMY_FLOW_FIELD) return plankton::UpdatesFlow(effect);
            else return plankton::UpdatesField(effect, field);
        };
        
        auto newHistory = plankton::MakeSharedMemory(now.node->Decl(), config.GetFlowValueType(), factory);
        auto& nowValue = GetFieldValue(now, field);
        auto& pastValue = GetFieldValue(past, field);
        auto& histValue = GetFieldValue(*newHistory, field);

        auto vector = plankton::MakeVector<EExpr>(interference.size() + 1);
        vector.push_back(
                // 'field' was never changed
                encoding.Encode(nowValue) == encoding.Encode(pastValue) &&
                encoding.Encode(pastValue) == encoding.Encode(histValue) && // redundant
                encoding.EncodeMemoryEquality(past, *newHistory)
        );
        for (const auto& effect : interference) {
            if (!updatesField(*effect)) continue;
            auto& effectValue = GetFieldValue(*effect->post, field);
            vector.push_back(
                    // last update to 'field' is due to 'effect'
                    encoding.Encode(nowValue) != encoding.Encode(pastValue) &&
                    encoding.Encode(nowValue) == encoding.Encode(histValue) &&
                    encoding.Encode(effectValue) == encoding.Encode(histValue) && // redundant
                    encoding.Encode(*effect->context) &&
                    encoding.EncodeMemoryEquality(*effect->post, *newHistory)
            );
        }
        
        preparation.emplace_back(encoding.MakeOr(vector), std::move(newHistory));
    }
    
    void PrepareField(const SharedMemoryCore& now, const SharedMemoryCore& past, const std::string& field) {
        auto& nowValue = GetFieldValue(now, field);
        auto& pastValue = GetFieldValue(past, field);
        if (nowValue == pastValue) return;
        PrepareFieldCheck(now, past, field);
    }
    
    void Prepare() {
        for (const auto* nowMem : plankton::Collect<SharedMemoryCore>(*annotation.now)) {
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
        if (preparation.empty()) return;
        DEBUG("=== Semantic Interpolation for: " << annotation << std::endl)
        
        // move past predicates into annotation, remember their symbols
        auto premises = plankton::MakeVector<std::pair<EExpr, std::set<const SymbolDeclaration*>>>(preparation.size());
        for (auto& [premise, past] : preparation) {
            premises.emplace_back(premise, plankton::Collect<SymbolDeclaration>(*past));
            annotation.Conjoin(std::make_unique<PastPredicate>(std::move(past)));
        }
        
        // create candidates, tabulate their symbols
        auto candidates = plankton::MakeStackCandidates(plankton::Collect<SymbolDeclaration>(annotation), POLICY);
        auto candidateSymbols = plankton::MakeVector<std::set<const SymbolDeclaration*>>(candidates.size());
        for (const auto& candidate : candidates) {
            candidateSymbols.push_back(plankton::Collect<SymbolDeclaration>(*candidate));
        }
        
        // encode candidate check
        std::vector<bool> eureka(candidates.size(), false);
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            auto encoded = encoding.Encode(*candidates.at(index));
            auto& symbols = candidateSymbols.at(index);
            for (const auto& [premise, others] : premises) {
                if (plankton::EmptyIntersection(symbols, others)) continue;
                encoding.AddCheck(premise >> encoded, [&eureka,index](bool holds){
                    if (holds) eureka.at(index) = true;
                });
            }
        }
        
        // find implied candidates
        encoding.Check();
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            if (!eureka.at(index)) continue;
            annotation.Conjoin(std::move(candidates.at(index)));
        }
        DEBUG("=== Semantic Interpolation yields: " << annotation << std::endl)
    }
    
    void Filter(const Type& nodeType) {
        plankton::InlineAndSimplify(annotation);
        std::set<std::pair<const LogicObject*, const LogicObject*>> subsumption;
        
        auto dummy = plankton::MakeSharedMemory(factory.GetFreshFO(nodeType), config.GetFlowValueType(), factory);
        auto symbols = plankton::Collect<SymbolDeclaration>(annotation);
        plankton::InsertInto(plankton::Collect<SymbolDeclaration>(*dummy), symbols);
        auto candidates = plankton::MakeStackCandidates(symbols, POLICY); // TODO: FULL?
    
        for (const auto& elem : candidates) {
            auto candidate = encoding.Encode(*elem);
            for (const auto& stronger : annotation.past) {
                if (stronger->formula->node->Type() != nodeType) continue;
                auto strongerEq = encoding.EncodeMemoryEquality(*stronger->formula, *dummy);
                for (const auto& weaker : annotation.past) {
                    if (weaker->formula->node->Decl() != stronger->formula->node->Decl()) continue;
                    subsumption.emplace(weaker.get(), stronger.get());
                    auto weakerEq = encoding.EncodeMemoryEquality(*weaker->formula, *dummy);
                    // check if annotation.now |= (weakerEq => candidate) => (strongerEq => candidate)
                    encoding.AddCheck((weakerEq >> candidate) >> (strongerEq >> candidate),
                                      [&subsumption, &weaker, &stronger](bool holds) {
                        if (!holds) subsumption.erase({weaker.get(), stronger.get()});
                    });
                }
            }
        }
        encoding.Check();
        
        std::set<const LogicObject*> prune;
        std::set<std::pair<const LogicObject*, const LogicObject*>> blacklist;
        for (const auto& [weaker, stronger] : subsumption) {
            if (plankton::Membership(blacklist, std::make_pair(stronger, weaker))) continue; // break circles
            blacklist.emplace(weaker, stronger);
            prune.insert(weaker);
        }
        plankton::RemoveIf(annotation.past,
                           [&prune](const auto& elem) { return plankton::Membership(prune, elem.get()); });
    }
    
    void Filter() {
        std::set<const Type*> ptrTypes;
        for (const auto& elem : annotation.past) ptrTypes.insert(&elem->formula->node->Type());
        for (const auto* type : ptrTypes) Filter(*type);
    }
};

void Solver::ImprovePast(Annotation& annotation) const {
    Interpolator(annotation, interference, config).Interpolate();
}
