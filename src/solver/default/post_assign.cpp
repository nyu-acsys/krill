#include "default_solver.hpp"

#include "z3++.h"
#include "eval.hpp"
#include "encoder.hpp"
#include "flowgraph.hpp"
#include "cola/util.hpp" // TODO: delete
#include "candidates.hpp"
#include "expand.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

static constexpr CandidateGenerator::FlowLevel POST_DERIVE_STACK_FLOW_LEVEL = CandidateGenerator::FlowLevel::FAST; // TODO: make configurable

/* NOTE:
 * This checks for acyclicity of the flow graph. One would prefer to check for non-empty flow cycles instead.
 * However, this would require to construct many flow graphs when extending the exploration frontier (detecting new points-to predicates).
 * Hence, we go with a syntactic check for reachability.
 */


struct ObligationFinder : public DefaultLogicListener {
    std::set<const ObligationAxiom*> result;
    void enter(const ObligationAxiom& obl) override { result.insert(&obl); }
};

std::set<const ObligationAxiom*> CollectObligations(const FlowGraph& footprint) {
    ObligationFinder finder;
    footprint.pre->now->accept(finder);
    return std::move(finder.result);
}

struct Reachability {
    private:
    std::map<const SymbolicVariableDeclaration*, std::set<const SymbolicVariableDeclaration*>> reachability;

    void Initialize(std::set<const SymbolicVariableDeclaration*>&& worklist, std::function<std::set<const SymbolicVariableDeclaration*>(const SymbolicVariableDeclaration&)>&& getNext) {
        // populate 1-step successors
        while (!worklist.empty()) {
            auto& decl = **worklist.begin();
            worklist.erase(worklist.begin());
            if (reachability.count(&decl) != 0) continue;
            reachability[&decl] = getNext(decl);
        }

        // populate (n+1)-step successors
        bool changed;
        do {
            changed = false;
            for (auto& [address, reach] : reachability) {
                auto& addressReach = reachability[address];
                auto oldSize = addressReach.size();
                for (auto* next : reach) {
                    if (address == next) continue;
                    auto& nextReach = reachability[next];
                    addressReach.insert(nextReach.begin(), nextReach.end());
                }
                changed |= oldSize != addressReach.size();
            }
        } while (changed);
    }

    public:
    explicit Reachability(const Formula& state) {
        LazyValuationMap eval(state);
        Initialize(heal::CollectMemoryNodes(state), [&eval](const auto& decl){
            std::set<const SymbolicVariableDeclaration*> successors;
            auto resource = eval.GetMemoryResourceOrNull(decl);
            if (resource) {
                for (const auto& [name, value] : resource->fieldToValue) {
                    if (value->Sort() != Sort::PTR) continue;
                    successors.insert(&value->Decl());
                }
            }
            return successors;
        });
    }

    explicit Reachability(const FlowGraph& graph, EMode mode) {
        Initialize({ &graph.GetRoot().address }, [&graph,&mode](const auto& decl){
            std::set<const SymbolicVariableDeclaration*> successors;
            auto node = graph.GetNodeOrNull(decl);
            if (node) {
                for (const auto& field : node->pointerFields) {
                    successors.insert(mode == EMode::PRE ? &field.preValue.get() : &field.postValue.get());
                }
            }
            return successors;
        });
    }

    bool IsReachable(const SymbolicVariableDeclaration& src, const SymbolicVariableDeclaration& dst) {
        return reachability[&src].count(&dst) != 0;
    }

    const std::set<const SymbolicVariableDeclaration*>& GetReachable(const SymbolicVariableDeclaration& src) {
        return reachability[&src];
    }
};

//
// Checks
//

struct FootprintChecks {
    using ContextMap = std::map<const VariableDeclaration*, SeparatingConjunction>;

    ImplicationCheckSet checks;
    std::set<const ObligationAxiom*> preSpec;
    std::deque<std::unique_ptr<SpecificationAxiom>> postSpec;
    ContextMap context;
    std::deque<std::unique_ptr<Axiom>> candidates;

    FootprintChecks(const FootprintChecks& other) = delete;
    explicit FootprintChecks(z3::context& context) : checks(context) {}
    void Add(const z3::expr& expr, std::function<void(bool)> callback) { checks.Add(expr, std::move(callback)); }
};

void CheckPublishing(FlowGraph& footprint) {
    for (auto& node : footprint.nodes) {
        if (node.preLocal == node.postLocal) continue;
        node.needed = true;
        for (auto& field : node.pointerFields) {
            auto next = footprint.GetNodeOrNull(field.postValue);
            if (!next) throw std::logic_error("Footprint too small to capture publishing"); // TODO: better error handling
            next->needed = true;
        }
    }
}

void CheckReachability(const FlowGraph& footprint) {
    Reachability preReach(footprint, EMode::PRE);
    Reachability postReach(footprint, EMode::POST);

    // ensure no cycle inside footprint after update (potential such cycles must involve root)
    for (const auto& node : footprint.nodes) {
        if (!postReach.IsReachable(node.address, node.address)) continue;
        throw std::logic_error("Cycle withing footprint detected. Cautiously aborting..."); // TODO: better error handling
    }

    // ensure that no nodes outside the footprint are reachable after the update that were unreachable before
    auto preRootReachable = preReach.GetReachable(footprint.GetRoot().address);
    for (const auto* address : postReach.GetReachable(footprint.GetRoot().address)) {
        if (preRootReachable.count(address) != 0) continue;
        if (footprint.GetNodeOrNull(*address)) continue;
        throw std::logic_error("Footprint too small to guarantee acyclicity of heap graph."); // TODO: better error handling
    }
}

void AddFlowCoverageChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    auto ensureFootprintContains = [&encoding](const SymbolicVariableDeclaration& address){
        auto mustHaveNode = encoding.graph.GetNodeOrNull(address);
        if (!mustHaveNode) throw std::logic_error("Footprint too small: does not cover all addresses the inflow of which changed"); // TODO: better error handling
        mustHaveNode->needed = true;
    };

    auto qv = encoding.encoder.QuantifiedVariable(encoding.graph.config.GetFlowValueType().sort);
    for (const auto& node : encoding.graph.nodes) {
        for (const auto& field : node.pointerFields) {
            auto sameFlow = encoding.encoder(field.preRootOutflow.value())(qv) == encoding.encoder(field.postRootOutflow.value())(qv);
            checks.Add(z3::forall(qv, sameFlow), [ensureFootprintContains,&node,&field](bool unchanged){
//                std::cout << "Checking outflow at " << node.address.name << ": unchanged=" << unchanged << std::endl;
                if (unchanged) return;
                ensureFootprintContains(node.address);
                ensureFootprintContains(field.preValue);
                ensureFootprintContains(field.postValue);
            });
        }
    }
}

//void AddFlowCycleChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
//    // Note: 'AddFlowCoverageChecks' guarantees that the update does not establish a non-empty flow cycle outside the footprint
//    // ensure absence of non-empty flow cycle inside footprint after update (such cycles must involve root)
//    for (const auto* field : encoding.graph.GetIncomingEdges(encoding.graph.GetRoot(), EMode::POST)) {
//        auto& flowField = field->postRootOutflow; // TODO: root or all flow?
//        assert(flowField.has_value());
//        InflowEmptinessAxiom tmp(flowField.value(), true);
//        checks.Add(encoding.encoder(tmp), [](bool noFlow){
//            if (noFlow) return;
//            throw std::logic_error("Potential flow cycle in footprint detected."); // TODO: better error handling
//        });
//    }
//}

void AddFlowUniquenessChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    auto disjointness = encoding.EncodeKeysetDisjointness(EMode::POST);
    checks.Add(disjointness, [](bool holds){
//        std::cout << "Checking keyset disjointness: holds=" << holds << std::endl;
        if (holds) return;
        throw std::logic_error("Unsafe update: keyset disjointness not guaranteed."); // TODO: better error handling
    });

    auto uniqueness = encoding.EncodeInflowUniqueness(EMode::POST);
    checks.Add(uniqueness, [](bool holds){
//        std::cout << "Checking inflow uniqueness: holds=" << holds << std::endl;
        if (holds) return;
        throw std::logic_error("Unsafe update: inflow uniqueness not guaranteed."); // TODO: better error handling
    });
}

void AddSpecificationChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
//    auto obligation = GetObligationOrNull(encoding.graph);
//    checks.obligation = obligation;

    bool mustBePure = checks.preSpec.empty();
    auto qv = encoding.encoder.QuantifiedVariable(encoding.graph.config.GetFlowValueType().sort);
    auto contains = [&encoding](auto& node, auto var, auto mode) {
        return encoding.EncodeNodeContains(node, var, mode);
    };

    // check purity
    z3::expr_vector preContains(encoding.encoder.context);
    z3::expr_vector postContains(encoding.encoder.context);
    for (const auto& node : encoding.graph.nodes) {
        preContains.push_back(encoding.encoder(node.preKeyset)(qv) && contains(node, qv, EMode::PRE));
        postContains.push_back(encoding.encoder(node.postKeyset)(qv) && contains(node, qv, EMode::POST));
    }
    auto isPure = z3::forall(qv, z3::mk_or(preContains) == z3::mk_or(postContains));
    checks.Add(isPure, [mustBePure,&checks](bool isPure){
//        std::cout << "Checking purity: isPure=" << isPure << std::endl;
        if (!isPure && mustBePure) throw std::logic_error("Unsafe update: impure update without obligation."); // TODO: better error handling
        if (!isPure) return;
        for (const auto* obligation : checks.preSpec) {
            checks.postSpec.push_back(heal::Copy(*obligation));
        }
    });

    // impure updates require obligation, checked together with purity
    for (const auto* obligation : checks.preSpec){
        assert(obligation);
        auto key = encoding.encoder(*obligation->key);

        // prepare
        z3::expr_vector othersPreContained(encoding.encoder.context);
        z3::expr_vector othersPostContained(encoding.encoder.context);
        z3::expr_vector keyPreContained(encoding.encoder.context);
        z3::expr_vector keyPostContained(encoding.encoder.context);
        for (const auto &node : encoding.graph.nodes) {
            auto preKeys = encoding.encoder(node.preKeyset);
            auto postKeys = encoding.encoder(node.postKeyset);
            othersPreContained.push_back(preKeys(qv) && contains(node, qv, EMode::PRE));
            othersPostContained.push_back(postKeys(qv) && contains(node, qv, EMode::POST));
            keyPreContained.push_back(preKeys(key) && contains(node, key, EMode::PRE));
            keyPostContained.push_back(postKeys(key) && contains(node, key, EMode::POST));
        }

        // check pure
        auto isContained = isPure && z3::mk_or(keyPreContained);
        auto isNotContained = isPure && !z3::mk_or(keyPreContained);
        checks.Add(isContained, [&checks, obligation](bool holds) {
            if (!holds || obligation->kind == SpecificationAxiom::Kind::DELETE) return;
            bool fulfilled = obligation->kind == SpecificationAxiom::Kind::CONTAINS; // key contained => contains: success, insertion: fail
            checks.postSpec.push_back(std::make_unique<FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), fulfilled));
        });
        checks.Add(isNotContained, [&checks, obligation](bool holds) {
            if (!holds || obligation->kind == SpecificationAxiom::Kind::INSERT) return;
            bool fulfilled = false; // key not contained => contains: fail, deletion: fail
            checks.postSpec.push_back(std::make_unique<FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), fulfilled));
        });

        // check impure
        if (obligation->kind == SpecificationAxiom::Kind::CONTAINS) continue; // always pure
        auto othersUnchanged = z3::forall(qv, z3::implies(qv != key, z3::mk_or(othersPreContained) == z3::mk_or(othersPostContained)));
        auto isInsertion = !z3::mk_or(keyPreContained) && z3::mk_or(keyPostContained) && othersUnchanged;
        auto isDeletion = z3::mk_or(keyPreContained) && !z3::mk_or(keyPostContained) && othersUnchanged;
        auto isUpdate = obligation->kind == SpecificationAxiom::Kind::INSERT ? isInsertion : isDeletion;
        checks.Add(isUpdate, [&checks, obligation](bool isTarget) {
//        std::cout << "Checking impure specification wrt obligation: holds=" << isTarget << std::endl;
            if (isTarget) {
                checks.postSpec.push_back(std::make_unique<FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), true));
            } else if (checks.postSpec.empty()) {
                // if the update was pure, then there was an obligation in checks.postSpec
                // hence, the update must be impure and it does not satisfy the spec
                throw std::logic_error("Unsafe update: impure update that does not satisfy the specification");
            }
        });
    }
}

void AddInvariantChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    for (const auto& node : encoding.graph.nodes) {
        auto nodeInvariant = encoding.EncodeNodeInvariant(node, EMode::POST);
        checks.Add(nodeInvariant, [](bool holds){
//            std::cout << "Checking invariant for " << node.address.name << ": holds=" << holds << std::endl;
            if (holds) return;
            throw std::logic_error("Unsafe update: invariant is not maintained."); // TODO: better error handling
        });
    }
}

void AddDerivedKnowledgeChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    // TODO: does this work?
    checks.candidates = CandidateGenerator::Generate(*encoding.graph.pre, POST_DERIVE_STACK_FLOW_LEVEL);
    for (std::size_t index = 0; index < checks.candidates.size(); ++index) {
        checks.Add(encoding.encoder(*checks.candidates[index]), [&checks,&encoding,index](bool holds){
            if (!holds) return;
            encoding.graph.pre->now->conjuncts.push_back(std::move(checks.candidates[index]));
        });
    }
}

//
// Perform Checks
//

void PerformChecks(EncodedFlowGraph& encoding, const FootprintChecks& checks) {
    ComputeImpliedCallback(encoding.solver, checks.checks);
}

void MinimizeFootprint(FlowGraph& footprint) {
//    footprint.nodes.erase(std::remove_if(footprint.nodes.begin(), footprint.nodes.end(),
//                                         [](const auto& node) { return !node.needed; }), footprint.nodes.end());
    std::deque<FlowGraphNode> nodes;
    for (auto& node : footprint.nodes) {
        if (!node.needed) continue;
        nodes.push_back(std::move(node));
    }
    footprint.nodes = std::move(nodes);
}

//
// Post Image
//

struct PostUpdater : DefaultLogicNonConstListener {
    FlowGraph& footprint;
    explicit PostUpdater(FlowGraph& footprint) : footprint(footprint) {}

    void enter(PointsToAxiom& resource) override {
        auto node = footprint.GetNodeOrNull(resource.node->Decl());
        if (!node) return;
        auto update = node->ToLogic(EMode::POST);
        resource.flow = update->flow;
        resource.isLocal = update->isLocal;
        resource.fieldToValue = std::move(update->fieldToValue);
    }
};

struct ObligationRemover : public DefaultLogicNonConstListener {
    std::set<const ObligationAxiom*> destroy;
    explicit ObligationRemover(std::set<const ObligationAxiom*> destroy) : destroy(std::move(destroy)) {}

    void enter(SeparatingConjunction& obj) override {
        obj.conjuncts.erase(std::remove_if(obj.conjuncts.begin(), obj.conjuncts.end(), [this](const auto& elem){
            auto obligation = dynamic_cast<const ObligationAxiom*>(elem.get()); // TODO: avoid cast
            return obligation && destroy.count(obligation) != 0;
        }), obj.conjuncts.end());
    }
};

std::unique_ptr<Annotation> ExtractPost(EncodedFlowGraph&& encoding, FootprintChecks&& checks) {
    PostUpdater updater(encoding.graph);
    ObligationRemover remover(std::move(checks.preSpec));

    auto result = std::move(encoding.graph.pre);
    result->now->accept(updater); // do not touch time predicates
    result->now->accept(remover); // do not touch time predicates
    std::move(checks.postSpec.begin(), checks.postSpec.end(), std::back_inserter(result->now->conjuncts));

    return result;
}

//
// Extract Effects
//

std::vector<std::function<std::unique_ptr<Axiom>()>> GetContextGenerators(const VariableDeclaration& decl) {
    if (auto symbol = dynamic_cast<const SymbolicVariableDeclaration*>(&decl)) {
        switch (decl.type.sort) {
            case Sort::VOID: return {};
            case Sort::BOOL: return { // true or false
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(true)); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(false)); }
            };
            case Sort::DATA: return { // min, max, or between
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicMin>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicMax>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()); }
            };
            case Sort::PTR: return { // NULL or not
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicNull>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::NEQ, std::make_unique<SymbolicNull>()); }
            };
        }
    } else if (auto flow = dynamic_cast<const SymbolicFlowDeclaration*>(&decl)) {
        return { // empty or not
            [flow]() { return std::make_unique<InflowEmptinessAxiom>(*flow, true); },
            [flow]() { return std::make_unique<InflowEmptinessAxiom>(*flow, false); }
        };
    }
    return {};
}

void AddEffectContextGenerators(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    // collect variables appearing in footprint
    std::set<const VariableDeclaration*> vars;
    for (const auto& node : encoding.graph.nodes) {
        auto dummy = node.ToLogic(EMode::PRE);
        vars.insert(&dummy->flow.get());
        for (const auto& [field, symbol] : dummy->fieldToValue) {
            vars.insert(&symbol->Decl());
        }
    }

    // test flow variables
    for (const auto* decl : vars) {
        for (auto&& generator : GetContextGenerators(*decl)) {
            auto candidate = encoding.encoder(*generator());
            checks.Add(candidate, [decl,generator,&checks](bool holds){
//                std::cout << "Testing "; heal::Print(*generator(), std::cout); std::cout << " ==> holds=" << holds << std::endl;
                if (holds) checks.context[decl].conjuncts.push_back(generator());
            });
        }
    }
}

std::unique_ptr<Formula> GetNodeUpdateContext(const PointsToAxiom& node, FootprintChecks::ContextMap& contextMap) {
    auto result = std::make_unique<SeparatingConjunction>();
    auto handle = [&result,&contextMap](const auto& decl) {
        auto knowledge = std::make_unique<SeparatingConjunction>();
        knowledge->conjuncts.swap(contextMap[&decl].conjuncts);
        result->conjuncts.push_back(std::move(knowledge));
    };

    handle(node.flow.get());
    for (const auto& [name, symbol] : node.fieldToValue) {
        handle(symbol->Decl());
    }

    heal::Simplify(*result);
    return result;
}

std::unique_ptr<HeapEffect> ExtractEffect(const FlowGraphNode& node, FootprintChecks::ContextMap& contextMap) {
    // ignore local nodes; after minimization, all remaining footprint nodes contain changes
    if (node.preLocal) return nullptr;
    auto pre = node.ToLogic(EMode::PRE);
    auto post = node.ToLogic(EMode::POST);
    auto context = GetNodeUpdateContext(*pre, contextMap);
    return std::make_unique<HeapEffect>(std::move(pre), std::move(post), std::move(context));
}

std::deque<std::unique_ptr<HeapEffect>> ExtractEffects(const EncodedFlowGraph& encoding, FootprintChecks::ContextMap&& contextMap) {
    // create a separate effect for every node
    std::deque<std::unique_ptr<HeapEffect>> result;
    for (const auto& node : encoding.graph.nodes) {
        auto effect = ExtractEffect(node, contextMap);
        if (!effect) continue;
        result.push_back(std::move(effect));
    }
    return result;
}

//
// Overall Algorithm (Heap Update)
//

PostImage DefaultSolver::PostMemoryUpdate(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs) const {
    // TODO: use future
    // TODO: create and use update/effect lookup table
    // TODO: start with smaller footprint and increase if too small?

//    // begin debug
//    std::cout << std::endl << std::endl << std::endl << "====================" << std::endl;
//    std::cout << "== Command: "; cola::print(cmd, std::cout);
//    std::cout << "== Pre: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl << std::flush;
//    // end debug

    auto [isVar, lhsVar] = heal::IsOfType<VariableExpression>(*lhs.expr);
    if (!isVar) throw std::logic_error("Unsupported assignment: dereference of non-variable"); // TODO: better error handling
    auto [isSimple, rhs] = heal::IsOfType<SimpleExpression>(*cmd.rhs);
    if (!isSimple) throw std::logic_error("Unsupported assignment: right-hand side is not simple"); // TODO:: better error handling

    FlowGraph footprint = solver::MakeFlowFootprint(std::move(pre), lhs, *rhs, Config());
    EncodedFlowGraph encoding(std::move(footprint));
    FootprintChecks checks(encoding.context);
    checks.preSpec = CollectObligations(encoding.graph);

    CheckPublishing(encoding.graph);
     CheckReachability(encoding.graph);
    AddFlowCoverageChecks(encoding, checks);
    // AddFlowCycleChecks(encoding, checks);
    AddFlowUniquenessChecks(encoding, checks);
    AddSpecificationChecks(encoding, checks);
    AddInvariantChecks(encoding, checks);
    AddDerivedKnowledgeChecks(encoding, checks);
    AddEffectContextGenerators(encoding, checks);
    PerformChecks(encoding, checks);

    MinimizeFootprint(encoding.graph);
    auto effects = ExtractEffects(encoding, std::move(checks.context));
    auto post = ExtractPost(std::move(encoding), std::move(checks));

    // begin debug
    heal::Print(*post, std::cout); std::cout << std::endl;
//    std::cout << "== Post: " << std::endl; heal::Print(*post, std::cout); std::cout << std::endl;
//    std::cout << "== Effects: " << std::endl;
//    for (auto& effect : effects) {
//        std::cout << "   - ";
//        heal::Print(*effect->pre, std::cout);
//        std::cout << "  ~~~>  ";
//        heal::Print(*effect->post, std::cout);
//        std::cout << "  UNDER  ";
//        heal::Print(*effect->context, std::cout);
//        std::cout << std::endl;
//    }
    // end debug

    return PostImage(std::move(post), std::move(effects));
}



//
// Overall Algorithm (Variable Update)
//

struct PointsToRemover : DefaultLogicNonConstListener {
    const std::set<const PointsToAxiom*>& prune;
    explicit PointsToRemover(const std::set<const PointsToAxiom*>& prune) : prune(prune) {}
    inline void Handle(decltype(SeparatingConjunction::conjuncts)& conjuncts) {
        conjuncts.erase(std::remove_if(conjuncts.begin(), conjuncts.end(), [this](const auto& elem){
            return prune.count(dynamic_cast<const PointsToAxiom*>(elem.get())) != 0;
//            if (auto pointsTo = dynamic_cast<const PointsToAxiom*>(elem.get())) {
//                return prune.count(&pointsTo->node->Decl()) != 0;
//            }
//            return false;
        }), conjuncts.end());
    }
    void enter(SeparatingConjunction& obj) override { Handle(obj.conjuncts); }
};

inline std::unique_ptr<PointsToAxiom> MakePointsTo(const SymbolicVariableDeclaration& address, SymbolicFactory& factory, const SolverConfig& config) {
    bool isLocal = false; // TODO: is that correct?
    auto& flow = factory.GetUnusedFlowVariable(config.GetFlowValueType());
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    for (const auto& [name, type] : address.type.fields) {
        fieldToValue.emplace(name, std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type)));
    }
    return std::make_unique<PointsToAxiom>(std::make_unique<SymbolicVariable>(address), isLocal, flow, std::move(fieldToValue));
}

inline bool IsGuaranteedNotEqual(const SymbolicVariableDeclaration& address, const SymbolicVariableDeclaration& compare, const Formula& state) {
    auto memory = FindMemory(address, state);
    auto other = FindMemory(compare, state);
    if (!memory || !other) return false;
    return &memory->node->Decl() != &other->node->Decl();
}

inline std::unique_ptr<SeparatingConjunction>
ExpandMemoryFrontier(std::unique_ptr<SeparatingConjunction> state, const SymbolicVariableDeclaration& addressAlias,
                     const Expression& rhs, SymbolicFactory& factory, const SolverConfig& config) {
    // only relevant for pointer fields
    if (rhs.sort() != Sort::PTR) return state;

    // do nothing if the memory resource is already present
    if (FindMemory(addressAlias, *state)) return state;

    // prepare for solving
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    ImplicationCheckSet checks(context);

    // extend and encode state
    for (const auto* node : heal::CollectMemory(*state)) {
        state = heal::Conjoin(std::move(state), config.GetNodeInvariant(*node));
    }
    for (const auto* var : heal::CollectVariables(*state)) {
        if (!var->variable->decl.is_shared) continue;
        state = heal::Conjoin(std::move(state), config.GetSharedVariableInvariant(*var));
    }
    solver.add(encoder(*state));

    // query info about the expansion
    auto address = std::cref(addressAlias);
    bool addressIsDefinitelyShared = false;
    if (auto dereference = dynamic_cast<const Dereference*>(&rhs)) {
        if (auto memory = FindResource(*dereference, *state)) { // TODO: lazy evaluation?
            address = memory->fieldToValue.at(dereference->fieldname)->Decl();
            addressIsDefinitelyShared = !memory->isLocal;
        }
    } else if (auto variable = dynamic_cast<const VariableExpression*>(&rhs)) {
        if (auto resource = FindResource(variable->decl, *state)) { // TODO: lazy evaluation?
            address = resource->value->Decl();
            addressIsDefinitelyShared = variable->decl.is_shared;
        }
    }

    // check for NULL and non-NULL
    std::optional<bool> addressIsNonNull = std::nullopt;
    auto isNull = std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(address), SymbolicAxiom::EQ, std::make_unique<SymbolicNull>());
    auto isNonNull = std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(address), SymbolicAxiom::NEQ, std::make_unique<SymbolicNull>());
    checks.Add(encoder(*isNull), [&addressIsNonNull](bool holds){ if (holds) addressIsNonNull = false; });
    checks.Add(encoder(*isNonNull), [&addressIsNonNull](bool holds){ if (holds) addressIsNonNull = true; });
    solver::ComputeImpliedCallback(solver, checks);
    if (addressIsNonNull) state->conjuncts.push_back(addressIsNonNull.value() ? std::move(isNonNull) : std::move(isNull));
    std::cout << " - expanding memory for address " << address.get().name << " (shared=" << addressIsDefinitelyShared << ")" << std::endl;
    std::cout << "   ==> nonnull = " << (addressIsNonNull ? std::to_string(addressIsNonNull.value()) : "?") << std::endl;

    // introduce new points-to predicate
    if (addressIsNonNull && addressIsNonNull.value()) {
        // find addresses that are guaranteed to be different from 'address'
        Reachability reachability(*state);
        auto potentiallyOverlapping = heal::CollectMemory(*state);
        for (auto it = potentiallyOverlapping.begin(); it != potentiallyOverlapping.end();) {
            bool isDistinct = (addressIsDefinitelyShared && (*it)->isLocal)
                              || reachability.IsReachable((*it)->node->Decl(), address)
                              || IsGuaranteedNotEqual(address, (*it)->node->Decl(), *state);
            std::cout << "   ==> address " << (*it)->node->Decl().name << " is distinct" << std::endl;
            if (isDistinct) it = potentiallyOverlapping.erase(it);
            else {
                if ((*it)->isLocal) throw std::logic_error("Expansion failed due to local memory"); // TODO: better error handling
                ++it;
            }
        }

        // delete resources that may overlap with 'address'
        if (!potentiallyOverlapping.empty()) {
            PointsToRemover remover(potentiallyOverlapping);
            state->accept(remover);
        }

        std::cout << "   ==> introducing new points to for address " << address.get().name << std::endl;
        auto newPointsTo = MakePointsTo(address, factory, config);
        auto invariant = config.GetNodeInvariant(*newPointsTo);
        state->conjuncts.push_back(std::move(newPointsTo));
        state = heal::Conjoin(std::move(state), std::move(invariant));
    }

    return state;
}

std::unique_ptr<Annotation> TryAddPureFulfillment(std::unique_ptr<Annotation> annotation, const SymbolicVariableDeclaration& address, const SolverConfig& config) {
    EncodedFlowGraph encoding(solver::MakeFlowGraph(std::move(annotation), address, config));
    FootprintChecks checks(encoding.context);
    checks.preSpec = CollectObligations(encoding.graph);
    AddSpecificationChecks(encoding, checks);
    AddDerivedKnowledgeChecks(encoding, checks);
    PerformChecks(encoding, checks);
    annotation = std::move(encoding.graph.pre);
    ObligationRemover remover(std::move(checks.preSpec));
    annotation->now->accept(remover); // do not touch time predicates
    std::move(checks.postSpec.begin(), checks.postSpec.end(), std::back_inserter(annotation->now->conjuncts));
    return annotation;
}

std::unique_ptr<Annotation> TryAddPureFulfillment(std::unique_ptr<Annotation> annotation, const Expression& expression, const SolverConfig& config) {
    if (expression.sort() != Sort::PTR) return annotation;
    if (auto dereference = dynamic_cast<const Dereference *>(&expression)) {
        auto resource = solver::FindResource(*dereference, *annotation);
        if (!resource) return annotation;
        return TryAddPureFulfillment(std::move(annotation), resource->node->Decl(), config);
    }
    return annotation;
}

PostImage DefaultSolver::PostVariableUpdate(std::unique_ptr<Annotation> pre, const Assignment& cmd, const VariableExpression& lhs) const {
    if (lhs.decl.is_shared)
        throw std::logic_error("Unsupported assignment: cannot assign to shared variables."); // TODO: better error handling

//    begin debug
//    std::cout << std::endl << std::endl << std::endl << "====================" << std::endl;
//    std::cout << "== Command: "; cola::print(cmd, std::cout);
//    std::cout << "== Pre: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl << std::flush;
//    end debug

    // perform update
    SymbolicFactory factory(*pre);
    auto value = EagerValuationMap(*pre).Evaluate(*cmd.rhs);
    auto& symbol = factory.GetUnusedSymbolicVariable(lhs.type());
    auto* resource = FindResource(lhs.decl, *pre->now);
    if (!resource) throw std::logic_error("Unsafe update: variable '" + lhs.decl.name + "' is not accessible."); // TODO: better error handling
    resource->value = std::make_unique<SymbolicVariable>(symbol);
    pre->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(symbol), SymbolicAxiom::EQ, std::move(value)));

    // try derive helpful knowledge
    pre->now =  ExpandMemoryFrontier(std::move(pre->now), symbol, *cmd.rhs, factory, Config());
    pre = TryAddPureFulfillment(std::move(pre), *cmd.rhs, Config());

//    begin debug
//    std::cout << "== Post: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl;
//    end debug

    heal::InlineAndSimplify(*pre->now);
    heal::Print(*pre, std::cout); std::cout << std::endl << std::endl;
    return PostImage(std::move(pre)); // local variable update => no effect
}

/*
 * Histories: wenn predecessor mal flow gehabt + $interpolationsargument, dann hat successor/'value' auch mal flow gehabt
 */
