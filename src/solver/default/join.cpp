#include "default_solver.hpp"

#include <numeric>
#include "z3++.h"
#include "encoder.hpp"
#include "eval.hpp"
#include "candidates.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


struct VariableCollector : public DefaultLogicListener {
    std::set<const VariableDeclaration*> result;
    void enter(const EqualsToAxiom& obj) override { result.insert(&obj.variable->decl); }
    static decltype(result) Collect(const Annotation& annotation) {
        VariableCollector collector;
        annotation.now->accept(collector);
        return std::move(collector.result);
    }
};

struct LocalMemoryCounter : public DefaultLogicListener {
    std::size_t result = 0;
    void enter(const PointsToAxiom& obj) override { result += obj.isLocal ? 1 : 0; }
    static decltype(result) Count(const Annotation& annotation) {
        LocalMemoryCounter counter;
        annotation.now->accept(counter);
        return counter.result;
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

class AnnotationJoiner {
    std::unique_ptr<Annotation> result;
    std::vector<std::unique_ptr<Annotation>> annotations;
    SymbolicFactory factory;
    std::map<const Annotation*, AnnotationInfo> lookup; // TODO: does this need to be a map? or is a vector sufficient?
    const SolverConfig& config;
    std::map<const VariableDeclaration*, bool> commonMemory;
    std::map<const VariableDeclaration*, std::unique_ptr<EqualsToAxiom>> varToCommonRes;
    std::map<const VariableDeclaration*, std::unique_ptr<PointsToAxiom>> varToCommonMem;
    z3::context context;
    z3::solver solver;
    Z3Encoder encoder;

    explicit AnnotationJoiner(std::vector<std::unique_ptr<Annotation>>&& annotations_, const SolverConfig& config)
            : result(std::make_unique<Annotation>()), annotations(std::move(annotations_)), config(config), solver(context), encoder(context, solver) {
        // TODO: what about histories and futures???

        assert(annotations.size() > 1);
        MakeSymbolsUnique();

//        debug
//        std::cout << "Joining: ";
//        for (const auto& elem : annotations) { heal::Print(*elem, std::cout); }
//        std::cout << std::endl;
//        debug end

        InitializeLookup();
        CheckCommonVariableResources();
        CreateVariableResources();
        FindCommonMemoryResources();
        CheckCommonMemoryResources();
        CreateMemoryResources();
        CreateSpecificationResources();
        EncodeAnnotations();
        EncodeAdditionalKnowledge();
        PrepareResult();
        DeriveJoinedStack();

//        debug
//        std::cout << "Result: "; heal::Print(*result, std::cout); std::cout << std::endl;
//        end debug
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
        for (const auto& [var, _ignored] : varToCommonRes) {
            if (var->type.sort != Sort::PTR) continue;

            bool hasCommonResource = true;
            std::optional<bool> isLocal;
            std::set<const SymbolicVariableDeclaration *> references;

            auto isCommon = [&](const auto* memory) {
                if (!memory) return false;
                if (blacklist.count(&memory->node->Decl()) != 0) return false;
                if (!isLocal.has_value()) isLocal = memory->isLocal;
                if (isLocal.value() != memory->isLocal) return false;
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

    void CheckCommonMemoryResources() {
//        if (config.applyInvariantToLocal) return; // TODO: is this correct?
        std::size_t countCommonLocal = std::transform_reduce(commonMemory.cbegin(), commonMemory.cend(), 0,
                                                             [](const auto& lhs, const auto& rhs){ return lhs + rhs; },
                                                             [](const auto& elem){ return elem.second ? 1 : 0; });
        for (const auto& annotation : annotations) {
            if (LocalMemoryCounter::Count(*annotation) == countCommonLocal) continue;
            throw std::logic_error("Unsupported join: looses local resource."); // TODO: better error handling
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
//        container.erase(std::remove_if(container.begin(), container.end(), UnaryPredicate), container.end());
        for (auto it = container.begin(); it != container.end();) {
            if (UnaryPredicate(*it)) it = container.erase(it);
            else ++it;
        }
    }

    void CreateSpecificationResources() {
        for (const auto& pair : varToCommonRes) {
            const auto* var = pair.first;
            std::set<SpecificationAxiom::Kind> obligations = lookup.begin()->second.varToObl[var];
            std::set<std::pair<SpecificationAxiom::Kind, bool>> fulfillments = lookup.begin()->second.varToFul[var];
            for (auto it = std::next(lookup.begin()); it != lookup.end(); ++it) {
                EraseIf(obligations, [&](const auto& elem){ return it->second.varToObl[var].count(elem) == 0; });
                EraseIf(fulfillments, [&](const auto& elem){ return it->second.varToFul[var].count(elem) == 0; });
            }

            const auto& key = pair.second->value->Decl();
            for (auto kind : obligations) {
                result->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(kind, std::make_unique<SymbolicVariable>(key)));
            }
            for (auto [kind, ret] : fulfillments) {
                result->now->conjuncts.push_back(std::make_unique<FulfillmentAxiom>(kind, std::make_unique<SymbolicVariable>(key), ret));
            }
        }
//        throw std::logic_error("does this work?");
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
        auto implied = solver::ComputeImplied(solver, encoded);

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
