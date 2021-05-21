#include "default_solver.hpp"

#include "z3++.h"
#include "encoder.hpp"
#include "eval.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

//template<typename Container, typename UnaryFunction>
//inline void ForAll(Container& container, const UnaryFunction& function) {
//    std::for_each(container.begin(), container.end(), function);
//}



struct VariableCollector : public DefaultLogicListener {
    std::set<const VariableDeclaration*> result;
    void enter(const EqualsToAxiom& obj) override { result.insert(&obj.variable->decl); }
    static decltype(result) Collect(const Annotation& annotation) {
        VariableCollector collector;
        annotation.now->accept(collector);
        return std::move(collector.result);
    }
};

inline const PointsToAxiom* GetMemoryOrNull(const VariableDeclaration& variable, const Annotation& annotation) {
    if (variable.type.sort != Sort::PTR) return nullptr;
    auto resource = FindResource(variable, *annotation.now);
    if (!resource) return nullptr;
    auto memory = FindMemory(resource->value->Decl(), *annotation.now);
    if (!memory) return nullptr;
    return memory;
}

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

struct JoinedResources {
    std::map<const VariableDeclaration*, std::unique_ptr<EqualsToAxiom>> variableToResource;
    std::map<const VariableDeclaration*, std::unique_ptr<PointsToAxiom>> variableToMemory;

    JoinedResources(const std::vector<std::unique_ptr<Annotation>>& annotations, SymbolicFactory& factory, const SolverConfig& config) {
        assert(!annotations.empty());
        auto variables = VariableCollector::Collect(*annotations.front());

        // check that annotations agree on variable resources // TODO: always be true when joining annotations from the same program location?
        for (auto it = std::next(annotations.begin()); it != annotations.end(); ++it) {
            auto otherVariables = VariableCollector::Collect(**it);
            if (variables != otherVariables) throw std::logic_error("Cannot join: number of variable resources mismatches");
        }

        // generate common resources
        for (const auto* variable : variables) {
            auto& symbol = factory.GetUnusedSymbolicVariable(variable->type);
            variableToResource[variable] = std::make_unique<EqualsToAxiom>(
                    std::make_unique<VariableExpression>(*variable), std::make_unique<SymbolicVariable>(symbol)
            );
            if (auto memory = GetMemoryOrNull(*variable, *annotations.front())) {
                variableToMemory[variable] = MakeCommonMemory(symbol, memory->isLocal, factory, config);
            }
        }

        // prune memory not accessible from all annotations
        for (auto it = std::next(annotations.begin()); it != annotations.end(); ++it) {
            for (const auto& [variable, resource] : variableToResource) {
                if (GetMemoryOrNull(*variable, **it)) continue;
                variableToMemory.erase(variable);
            }
        }
    }

//    static std::unique_ptr<SeparatingConjunction> ToLogic(JoinedResources&& resources) {
//        auto result = std::make_unique<SeparatingConjunction>();
//        for (auto& [var, axiom] : resources.variableToResource) {
//            result->conjuncts.push_back(std::move(axiom));
//        }
//        for (auto& [var, axiom] : resources.variableToMemory) {
//            result->conjuncts.push_back(std::move(axiom));
//        }
//        return result;
//    }
};

std::unique_ptr<Annotation> DefaultSolver::Join(std::vector<std::unique_ptr<Annotation>> annotations) const {
    if (annotations.empty()) return std::make_unique<Annotation>();
    if (annotations.size() == 1) return std::move(annotations.front());
    SymbolicFactory factory;

    // make symbolic variables/flows unique (1) within resources of each annotation (2) across annotations
    for (auto& annotation : annotations) {
        heal::RenameSymbolicSymbols(*annotation, factory);
    }

    // find common resources
    JoinedResources resources(annotations, factory, Config());

    // encode annotations with additional resource knowledge
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    z3::expr_vector encodedAnnotations(context);
    for (const auto& annotation : annotations) {
        encodedAnnotations.push_back(encoder(*annotation->now) && equalities); // TODO: encoder must encode EqualsToAxioms as equalities
    }

    // create candidates and solve

    // TODO: encode annotations + equalities resulting from resources (let encoder add equalities for EqualsToAxiom?)
    // TODO: check what disjunction of annotations implies (create list of candidate axioms to be added? this would prune implications...)


    // TODO: what about histories and futures??
    throw std::logic_error("not yet implemented");
}










struct AnnotationInfo {
    std::map<const VariableDeclaration*, const EqualsToAxiom*> varToRes;
    std::map<const VariableDeclaration*, const PointsToAxiom*> varToMem;

    explicit AnnotationInfo(const Annotation& annotation) {
        auto variables = VariableCollector::Collect(annotation);
        for (const auto* variable : variables) {
            auto resource = FindResource(*variable, annotation);
            if (!resource) continue;
            varToRes[variable] = resource;

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
        EncodeAnnotations();
        EncodeAdditionalKnowledge();
        PrepareResult();
        DeriveJoinedStack();
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
        for (const auto* var : VariableCollector::Collect(*annotations.front())) {
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
        throw std::logic_error("not yet implemented");
    }

public:
    static std::unique_ptr<Annotation> Join(std::vector<std::unique_ptr<Annotation>>&& annotations, const SolverConfig& config) {
        if (annotations.empty()) return std::make_unique<Annotation>();
        if (annotations.size() == 1) return std::move(annotations.front());
        AnnotationJoiner join(std::move(annotations), config);
        return std::move(join.result);
    }
};







