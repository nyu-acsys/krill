#include "default_solver.hpp"

#include "z3++.h"
#include "encoder.hpp"
#include "eval.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


//
// Candidates
//

inline auto MkVar(const SymbolicVariableDeclaration& decl) { return std::make_unique<SymbolicVariable>(decl); }

struct CandidateGenerator : public DefaultLogicListener {
    std::set<const SymbolicVariableDeclaration*> symbols;
    std::set<const SymbolicFlowDeclaration*> flows;
    std::deque<std::unique_ptr<Axiom>> result;

    void enter(const SymbolicVariable& obj) override { symbols.insert(&obj.Decl()); }
    void enter(const PointsToAxiom& obj) override { flows.insert(&obj.flow.get()); }
    void enter(const InflowContainsValueAxiom& obj) override { flows.insert(&obj.flow.get()); }
    void enter(const InflowContainsRangeAxiom& obj) override { flows.insert(&obj.flow.get()); }
    void enter(const InflowEmptinessAxiom& obj) override { flows.insert(&obj.flow.get()); }

    explicit CandidateGenerator(const LogicObject& base) {
        base.accept(*this);

        for (const auto* flow : flows) {
            AddUnaryFlowCandidates(*flow);
        }
        for (auto symbol = symbols.begin(); symbol != symbols.end(); ++symbol) {
            AddUnarySymbolCandidates(**symbol);
            for (const auto* flow : flows) {
                AddBinaryFlowCandidates(*flow, **symbol);
            }
            for (auto other = std::next(symbol); other != symbols.end(); ++other) {
                AddBinarySymbolCandidates(**symbol, **other);
                AddBinarySymbolCandidates(**other, **symbol);
                for (const auto* flow : flows) {
                    AddTernaryFlowCandidates(*flow, **symbol, **other);
                    AddTernaryFlowCandidates(*flow, **other, **symbol);
                }
            }
        }
    }

    inline static const std::vector<SymbolicAxiom::Operator>& GetOps(Sort sort) {
        static std::vector<SymbolicAxiom::Operator> ptrOps = { SymbolicAxiom::EQ, SymbolicAxiom::NEQ };
        static std::vector<SymbolicAxiom::Operator> dataOps = { SymbolicAxiom::EQ, SymbolicAxiom::NEQ, SymbolicAxiom::LEQ, SymbolicAxiom::LT, SymbolicAxiom::GEQ, SymbolicAxiom::GT };
        return sort != Sort::DATA ? ptrOps : dataOps;
    }

    inline static std::vector<std::unique_ptr<SymbolicExpression>> GetImmis(Sort sort) {
        std::vector<std::unique_ptr<SymbolicExpression>> result;
        result.reserve(2);
        switch (sort) {
            case Sort::VOID: break;
            case Sort::BOOL: result.push_back(std::make_unique<SymbolicBool>(true)); result.push_back(std::make_unique<SymbolicBool>(false)); break;
            case Sort::DATA: result.push_back(std::make_unique<SymbolicMin>()); result.push_back(std::make_unique<SymbolicMax>()); break;
            case Sort::PTR: result.push_back(std::make_unique<SymbolicNull>()); break;
        }
        return result;
    }

    void AddUnarySymbolCandidates(const SymbolicVariableDeclaration& symbol) {
        for (auto op : GetOps(symbol.type.sort)) {
            for (auto&& immi : GetImmis(symbol.type.sort)) {
                result.push_back(std::make_unique<SymbolicAxiom>(MkVar(symbol), op, std::move(immi)));
            }
        }
    }

    void AddBinarySymbolCandidates(const SymbolicVariableDeclaration& symbol, const SymbolicVariableDeclaration& other) {
        if (symbol.type.sort != other.type.sort) return;
        for (auto op : GetOps(symbol.type.sort)) {
            result.push_back(std::make_unique<SymbolicAxiom>(MkVar(symbol), op, MkVar(other)));
        }
    }

    void AddUnaryFlowCandidates(const SymbolicFlowDeclaration& flow) {
        for (auto value : {true, false}) {
            result.push_back(std::make_unique<InflowEmptinessAxiom>(flow, false));
        }
    }

    void AddBinaryFlowCandidates(const SymbolicFlowDeclaration& flow, const SymbolicVariableDeclaration& symbol) {
        if (flow.type != symbol.type) return;
        result.push_back(std::make_unique<InflowContainsValueAxiom>(flow, MkVar(symbol)));
    }

    void AddTernaryFlowCandidates(const SymbolicFlowDeclaration& flow, const SymbolicVariableDeclaration& symbol, const SymbolicVariableDeclaration& other) {
        if (flow.type != symbol.type) return;
        if (flow.type != other.type) return;
        result.push_back(std::make_unique<InflowContainsRangeAxiom>(flow, MkVar(symbol), MkVar(other)));
    }

    static decltype(result) Generate(const LogicObject& base) {
        CandidateGenerator generator(base);
        return std::move(generator.result);
    }
};

//
// Joining
//

struct VariableCollector : public DefaultLogicListener {
    std::set<const VariableDeclaration*> result;
    void enter(const EqualsToAxiom& obj) override { result.insert(&obj.variable->decl); }
    static decltype(result) Collect(const Annotation& annotation) {
        VariableCollector collector;
        annotation.now->accept(collector);
        return std::move(collector.result);
    }
};

inline std::unique_ptr<PointsToAxiom> MakeCommonMemory(const SymbolicVariableDeclaration& address, bool local, SymbolicFactory& factory,
                                                       const SolverConfig& config) {
    auto node = std::make_unique<SymbolicVariable>(address);
    auto& flow = factory.GetUnusedFlowVariable(config.flowDomain->GetFlowValueType());
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& [name, type] : address.type.fields) {
        fields[name] = std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type));
    }
    return std::make_unique<PointsToAxiom>(std::move(node), local, flow, std::move(fields));
}

struct AnnotationInfo {
    std::map<const VariableDeclaration*, const EqualsToAxiom*> varToRes;
    std::map<const VariableDeclaration*, const PointsToAxiom*> varToMem;
    std::map<const VariableDeclaration*, std::set<SpecificationAxiom::Kind>> varToObl;
    std::map<const VariableDeclaration*, std::set<std::pair<SpecificationAxiom::Kind, bool>>> varToFul;

    explicit AnnotationInfo(const Annotation& annotation) {
        auto variables = VariableCollector::Collect(annotation);
        for (const auto* variable : variables) {
            auto resource = FindResource(*variable, annotation);
            if (!resource) continue;
            varToRes[variable] = resource;

            auto axioms = FindSpecificationAxioms(resource->value->Decl(), annotation);
            for (const auto* spec : axioms) {
                if (auto obl = dynamic_cast<const ObligationAxiom*>(spec)) {
                    varToObl[variable].emplace(obl->kind);
                } else if (auto ful = dynamic_cast<const FulfillmentAxiom*>(spec)) {
                    varToFul[variable].emplace(ful->kind, ful->return_value);
                }
            }

            if (variable->type.sort != Sort::PTR) continue;
            auto memory = FindMemory(resource->value->Decl(), annotation);
            if (!memory) continue;
            varToMem[variable] = memory;
        }
    }
};

using AnnotationInfoMap = std::map<const Annotation*, AnnotationInfo>;

class AnnotationJoiner {
    std::unique_ptr<Annotation> result;
    std::vector<std::unique_ptr<Annotation>> annotations;
    SymbolicFactory factory;
    AnnotationInfoMap lookup; // TODO: does this need to be a map? or is a vector sufficient?
    const SolverConfig& config;
    std::map<const VariableDeclaration*, bool> commonMemory;
    std::map<const VariableDeclaration*, std::unique_ptr<EqualsToAxiom>> varToCommonRes;
    std::map<const VariableDeclaration*, std::unique_ptr<PointsToAxiom>> varToCommonMem;
    z3::context context;
    z3::solver solver;
    Z3Encoder encoder;

    explicit AnnotationJoiner(std::vector<std::unique_ptr<Annotation>>&& annotations, const SolverConfig& config)
            : annotations(std::move(annotations)), config(config), solver(context), encoder(context, solver) {
        // TODO: what about histories and futures???
        assert(annotations.size() > 1);
        MakeSymbolsUnique();
        InitializeLookup();
        CheckCommonVariableResources();
        CreateVariableResources();
        FindCommonMemoryResources();
        CreateMemoryResources();
        CreateSpecificationResources();
        EncodeAnnotations();
        EncodeAdditionalKnowledge();
        PrepareResult();
        DeriveJoinedStack();
        HandleSpecification();
    }

    void MakeSymbolsUnique() {
        for (auto& annotation : annotations) {
            heal::RenameSymbolicSymbols(*annotation, factory);
        }
    }

    void InitializeLookup() {
        for (const auto& annotation : annotations) {
            lookup.emplace(annotation.get(), *annotation);
        }
    }

    void CheckCommonVariableResources() {
        // TODO: always holds when joining annotations from the same program location?
        auto& firstInfo = lookup.begin()->second.varToRes;
        auto comparator = [](const auto& lhs, const auto& rhs){ return lhs.first == rhs.first; };
        for (auto it = std::next(lookup.begin()); it != lookup.end(); ++it) {
            auto& otherInfo = it->second.varToRes;
            bool equal = firstInfo.size() == otherInfo.size() && std::equal(firstInfo.begin(), firstInfo.end(), otherInfo.begin(), comparator);
            if (equal) continue;
            throw std::logic_error("Cannot join: number of variable resources mismatches");
        }
    }

    void CreateVariableResources() {
        for (const auto& [var, res] : lookup.begin()->second.varToRes) {
            auto& symbol = factory.GetUnusedSymbolicVariable(var->type);
            varToCommonRes[var] = std::make_unique<EqualsToAxiom>(
                    std::make_unique<VariableExpression>(*var), std::make_unique<SymbolicVariable>(symbol)
            );
        }
    }

    void FindCommonMemoryResources() {
        // memory resources must be unique => handle two variables pointing to the same memory
        std::set<const SymbolicVariableDeclaration*> blacklist;
//        for (const auto* var : VariableCollector::Collect(*annotations.front())) {
        for (const auto& [var, _ignored] : varToCommonRes) {
            bool hasCommonResource = true;
            std::optional<bool> isLocal;
            std::set<const SymbolicVariableDeclaration *> references;

            auto isCommon = [&](const auto* memory) {
                if (!memory) return false;
                if (blacklist.count(&memory->node->Decl()) != 0) return false;
                if (isLocal.has_value() && isLocal.value() != memory->isLocal) return false;
                return true;
            };
            for (const auto &annotation : annotations) {
                auto memory = lookup.at(annotation.get()).varToMem[var];
                if (!isCommon(memory)) {
                    hasCommonResource = false;
                    break;
                }
                references.insert(&memory->node->Decl());
            }

            if (!hasCommonResource || !isLocal.has_value()) continue;
            commonMemory.emplace(var, isLocal.value());
            blacklist.insert(references.begin(), references.end());
        }
    }

    void CreateMemoryResources() {
        for (const auto& [var, local] : commonMemory) {
            auto& address = varToCommonRes.at(var)->value->Decl();
            varToCommonMem[var] = MakeCommonMemory(address, local, factory, config);
        }
    }

    template<typename T, typename F>
    static inline void EraseIf(T& container, F UnaryPredicate) {
        container.erase(std::remove_if(container.begin(), container.end(), UnaryPredicate), container.end());
    }

    void CreateSpecificationResources() {
        throw std::logic_error("does this work?");
        for (const auto& pair : varToCommonRes) {
            const auto* var = pair.first;
            auto obligations = lookup.begin()->second.varToObl[var];
            auto fulfillments = lookup.begin()->second.varToFul[var];
            for (auto it = std::next(lookup.begin()); it != lookup.end(); ++it) {
                EraseIf(obligations, [&](const auto& elem){ it->second.varToObl[var].count(elem) == 0; });
                EraseIf(fulfillments, [&](const auto& elem){ it->second.varToFul[var].count(elem) == 0; });
            }

            const auto& key = pair.second->value->Decl();
            for (auto kind : obligations) {
                result->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(kind, MkVar(key)));
            }
            for (auto [kind, ret] : fulfillments) {
                result->now->conjuncts.push_back(std::make_unique<FulfillmentAxiom>(kind, MkVar(key), ret));
            }
        }
    }

    void EncodeAnnotations() {
        z3::expr_vector vector(context);
        for (const auto& annotation : annotations) {
            vector.push_back(encoder(*annotation->now));
        }
        solver.add(z3::mk_or(vector));
    }

    void EncodeAdditionalKnowledge() {
        // force variable valuations to agree
        for (const auto& [annotation, info] : lookup) {
            for (const auto& [var, res] : info.varToRes) {
                solver.add(encoder(*res->value) == encoder(*varToCommonRes.at(var)->value));
            }
        }

        // force common memory to agree
        auto qv = encoder.QuantifiedVariable(config.flowDomain->GetFlowValueType().sort);
        for (const auto& [annotation, info] : lookup) {
            for (const auto& [var, memory] : varToCommonMem) {
                auto other = info.varToMem.at(var);

                solver.add(encoder(*memory->node) == encoder(*other->node));
                solver.add(z3::forall(qv, encoder(memory->flow)(qv) == encoder(other->flow)(qv)));
                assert(memory->node->Type() == other->node->Type());
                for (const auto& [field, value] : memory->fieldToValue) {
                    solver.add(encoder(*value) == encoder(*other->fieldToValue.at(field)));
                }
            }
        }
    }

    void PrepareResult() {
        for (auto& [var, res] : varToCommonRes) result->now->conjuncts.push_back(std::move(res));
        for (auto& [var, res] : varToCommonMem) result->now->conjuncts.push_back(std::move(res));
        varToCommonRes.clear();
        varToCommonMem.clear();
    }

    void DeriveJoinedStack() {
        auto candidates = CandidateGenerator::Generate(*result);

        z3::expr_vector encoded(context);
        for (const auto& candidate : candidates) {
            encoded.push_back(encoder(*candidate));
        }
        auto implied = solver::ComputeImplied(context, solver, encoded);

        for (std::size_t index = 0; index < candidates.size(); ++index) {
            if (!implied.at(index)) continue;
            result->now->conjuncts.push_back(std::move(candidates.at(index)));
        }
    }

public:
    static std::unique_ptr<Annotation> Join(std::vector<std::unique_ptr<Annotation>>&& annotations, const SolverConfig& config) {
        if (annotations.empty()) return std::make_unique<Annotation>();
        if (annotations.size() == 1) return std::move(annotations.front());
        AnnotationJoiner join(std::move(annotations), config);
        return std::move(join.result);
    }
};

std::unique_ptr<Annotation> DefaultSolver::Join(std::vector<std::unique_ptr<Annotation>> annotations) const {
    return AnnotationJoiner::Join(std::move(annotations), Config());
}
