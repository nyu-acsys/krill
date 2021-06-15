#include "default_solver.hpp"

#include <numeric>
#include "z3++.h"
#include "encoder.hpp"
#include "eval.hpp"
#include "candidates.hpp"
#include "expand.hpp"
#include "heal/util.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


struct VariableCollector : public DefaultLogicListener {
    std::set<const VariableDeclaration*> result;
    void enter(const EqualsToAxiom& obj) override { result.insert(&obj.variable->decl); }
    static inline decltype(result) Collect(const Annotation& annotation) {
        VariableCollector collector;
        annotation.now->accept(collector);
        return std::move(collector.result);
    }
};

struct LocalMemoryCounter : public DefaultLogicListener {
    std::size_t result = 0;
    void enter(const PointsToAxiom& obj) override { result += obj.isLocal ? 1 : 0; }
    static inline decltype(result) Count(const Annotation& annotation) {
        LocalMemoryCounter counter;
        annotation.now->accept(counter);
        return counter.result;
    }
};

inline std::unique_ptr<PointsToAxiom> MakeCommonMemory(const SymbolicVariableDeclaration& address, bool local, SymbolicFactory& factory,
                                                       const SolverConfig& config) {
    auto node = std::make_unique<SymbolicVariable>(address);
    auto& flow = factory.GetUnusedFlowVariable(config.GetFlowValueType());
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& [name, type] : address.type.fields) {
        fields[name] = std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type));
    }
    return std::make_unique<PointsToAxiom>(std::move(node), local, flow, std::move(fields));
}

struct AnnotationInfo {
    std::map<const VariableDeclaration*, const EqualsToAxiom*> varToRes;
    std::map<const VariableDeclaration*, const PointsToAxiom*> varToMem;
    std::map<const VariableDeclaration*, std::set<const SpecificationAxiom*>> varToSpec;

    explicit AnnotationInfo(const Annotation& annotation) {
        auto variables = VariableCollector::Collect(annotation);
        for (const auto* variable : variables) {
            auto resource = FindResource(*variable, annotation);
            if (!resource) continue;
            varToRes[variable] = resource;

            auto specs = FindSpecificationAxioms(resource->value->Decl(), annotation);
            varToSpec[variable].insert(specs.begin(), specs.end());

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
    Z3Encoder<> encoder;

    explicit AnnotationJoiner(std::vector<std::unique_ptr<Annotation>>&& annotations_, const SolverConfig& config)
            : result(std::make_unique<Annotation>()), annotations(std::move(annotations_)), config(config), solver(context), encoder(context, solver) {

        // TODO: what about histories and futures???
        std::cerr << "WARNING: join loses time predicates" << std::endl;

        // prune false // TODO: remove this?
        ImplicationCheckSet checks(context);
        for (auto& annotation : annotations) {
            auto isFalse = z3::implies(encoder(*annotation), context.bool_val(false));
            checks.Add(isFalse, [&annotation](bool holds){ if (holds) annotation.reset(nullptr); });
        }
        solver::ComputeImpliedCallback(solver, checks);
        annotations.erase(std::remove_if(annotations.begin(), annotations.end(), [](const auto& elem){ return elem.get() == nullptr; }), annotations.end());
    }

    std::unique_ptr<Annotation> GetResult() {
        if (annotations.empty()) return std::make_unique<Annotation>(); // TODO: return false?
        if (annotations.size() == 1) return std::move(annotations.front());

        assert(annotations.size() > 1);
        MakeSymbolsUnique();
        AddInvariants();

        InitializeLookup();
        CheckCommonVariableResources();
        CreateVariableResources();
        FindCommonMemoryResources();
        CheckCommonMemoryResources();
        CreateMemoryResources();
        CreateSpecificationResources();
        Encode();
        PrepareResult();
        DeriveJoinedStack();
        return std::move(result);
    }

    void MakeSymbolsUnique() {
        for (auto& annotation : annotations) {
            heal::RenameSymbolicSymbols(*annotation, factory);
        }
    }

    void AddInvariants() {
        for (auto& annotation : annotations) {
            auto memories = heal::CollectMemory(*annotation->now);
            auto variables = heal::CollectVariables(*annotation->now);
            for (const auto* memory : memories) {
                annotation->now->conjuncts.push_back(config.GetNodeInvariant(*memory));
            }
            for (const auto* variable : variables) {
                if (!variable->variable->decl.is_shared) continue;
                annotation->now->conjuncts.push_back(config.GetSharedVariableInvariant(*variable));
            }
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
            std::set<const SymbolicVariableDeclaration*> references;

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
            throw std::logic_error("Unsupported join: loses local resource."); // TODO: better error handling
        }
    }

    void CreateMemoryResources() {
        for (const auto& [var, local] : commonMemory) {
            auto& address = varToCommonRes.at(var)->value->Decl();
            varToCommonMem[var] = MakeCommonMemory(address, local, factory, config);
        }
    }

    static inline bool EqualSpec(const SpecificationAxiom& axiom, const SpecificationAxiom& other) {
        if (axiom.kind != other.kind) return false;

        struct Comparator : public BaseLogicVisitor {
            std::optional<bool> returnValue = std::nullopt;
            void visit(const ObligationAxiom&) override { /* do nothing */ }
            void visit(const FulfillmentAxiom& axiom) override { returnValue = axiom.return_value; }
            static inline std::optional<bool> Get(const SpecificationAxiom& axiom) {
                Comparator cmp;
                axiom.accept(cmp);
                return cmp.returnValue;
            }
        };
        return Comparator::Get(axiom) == Comparator::Get(other);
    }

    void CreateSpecificationResources() {
//        throw std::logic_error("bug");

        std::set<const SpecificationAxiom*> blacklist;
        for (const auto& [variable, resource] : varToCommonRes) {
            auto specifications = lookup.begin()->second.varToSpec[variable];

            // prune specs if they are not mutual
            for (auto it = specifications.begin(); it != specifications.end();) {
                // check if all annotations cover it
                std::set<const SpecificationAxiom*> used;
                bool covered = true;
                for (auto& [annotation, info] : lookup) {
                    // check if one annotation covers it
                    bool found = false;
                    for (auto* spec : info.varToSpec[variable]) {
                        if (blacklist.count(spec) != 0) continue;
                        if (!EqualSpec(**it, *spec)) continue;
                        found = true;
                        used.insert(spec);
                        break;
                    }
                    if (found) continue;
                    covered = false;
                    break;
                }

                // remove if not covered, blacklist otherwise
                if (!covered) it = specifications.erase(it);
                else {
                    blacklist.insert(used.begin(), used.end());
                    ++it;
                }
            }

            // add remaining specifications
            for (auto* elem : specifications) {
                auto spec = heal::Copy(*elem);
                spec->key = heal::Copy(*resource->value);
                result->now->conjuncts.push_back(std::move(spec));
            }
        }
    }

    z3::expr_vector MakeDebugVector(const z3::expr& expr) {
        z3::expr_vector vector(context);
        vector.push_back(expr);
        return vector;
    }

    void Encode() {
        z3::expr_vector vector(context);

        for (const auto& [annotation, info] : lookup) {
            z3::expr_vector encoded(context);

            // encode annotation
            encoded.push_back(encoder(*annotation->now));

            // force common variables to agree
            for (const auto& [var, resource] : varToCommonRes) {
                auto other = info.varToRes.at(var);
                encoded.push_back(encoder(*resource->value) == encoder(*other->value));
            }

            // force common memory to agree
            for (const auto& [var, memory] : varToCommonMem) {
                auto other = info.varToMem.at(var);
                encoded.push_back(encoder.MakeMemoryEquality(*memory, *other));
            }

            vector.push_back(z3::mk_and(encoded));
        }

        solver.add(z3::mk_or(vector));
    }

    void PrepareResult() {
        for (auto& [var, res] : varToCommonRes) result->now->conjuncts.push_back(std::move(res));
        for (auto& [var, res] : varToCommonMem) result->now->conjuncts.push_back(std::move(res));
        varToCommonRes.clear();
        varToCommonMem.clear();

        // since we filtered out unsatisfiable annotations, we expect the join to be satisfiable (check may be costly)
        assert(solver.check(MakeDebugVector(encoder(*result))) == z3::sat);
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
        static size_t timeSpent = 0;
        auto start = std::chrono::steady_clock::now();

        std::cout << std::endl << std::endl << "========= joining " << annotations.size() << std::endl; for (const auto& elem : annotations) heal::Print(*elem, std::cout); std::cout << std::endl;
        auto result = AnnotationJoiner(std::move(annotations), config).GetResult();
        heal::InlineAndSimplify(*result);
        std::cout << "** join result: "; heal::Print(*result, std::cout); std::cout << std::endl << std::endl << std::endl;

        timeSpent += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::cout << "& All time spent in Join: " << timeSpent << "ms" << std::endl;
        assert(heal::CollectObligations(*result).size() + heal::CollectFulfillments(*result).size() > 0);
        return result;
    }
};

std::unique_ptr<Annotation> DefaultSolver::Join(std::vector<std::unique_ptr<Annotation>> annotations) const {
    if (annotations.empty()) throw std::logic_error("Empty join not supported"); // TODO: better error handling
    return AnnotationJoiner::Join(std::move(annotations), Config());
}
