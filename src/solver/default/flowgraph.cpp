#include "flowgraph.hpp"

#include "util/timer.hpp"
#include "eval.hpp"
#include "expand.hpp"

using namespace cola;
using namespace heal;
using namespace engine;


constexpr unsigned int MAX_INFLOW_PREDECESSORS = 1; // TODO: make this configurable via the formula/annotation itself

//
// Flow graph access
//

const FlowGraphNode* FlowGraph::GetNodeOrNull(const heal::SymbolicVariableDeclaration& address) const {
    // search for an existing node at the given address
    auto pred = [&address](const auto& item) { return &item.address == &address; };
    auto find = std::find_if(nodes.begin(), nodes.end(), pred);
    if (find != nodes.end()) return &(*find);
    return nullptr;
}

FlowGraphNode* FlowGraph::GetNodeOrNull(const heal::SymbolicVariableDeclaration& address) {
    return const_cast<FlowGraphNode*>(std::as_const(*this).GetNodeOrNull(address));
}

std::unique_ptr<heal::PointsToAxiom> FlowGraphNode::ToLogic(EMode mode) const {
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& field : pointerFields) {
        fields[field.name] = std::make_unique<SymbolicVariable>(mode == EMode::PRE ? field.preValue : field.postValue);
    }
    for (const auto& field : dataFields) {
        fields[field.name] = std::make_unique<SymbolicVariable>(mode == EMode::PRE ? field.preValue : field.postValue);
    }
    return std::make_unique<PointsToAxiom>(
            std::make_unique<SymbolicVariable>(address),
            mode == EMode::PRE ? preLocal : postLocal,
            mode == EMode::PRE ? preAllInflow : postAllInflow,
            std::move(fields)
    );
}

inline Field& GetFieldOrFail(FlowGraphNode& node, const std::string& name) {
    for (auto& field : node.pointerFields) {
        if (field.name == name) return field;
    }
    for (auto& field : node.dataFields) {
        if (field.name == name) return field;
    }
    throw std::logic_error("Could not find field."); // TODO: better error handling
}

//inline bool IsRoot(const FlowGraph& graph, const FlowGraphNode& node) {
//    return &node == &graph.nodes.front();
//}

std::vector<const Field*> FlowGraph::GetIncomingEdges(const FlowGraphNode& target, EMode mode) const {
    std::vector<const Field*> result;
    result.reserve(nodes.size() * target.pointerFields.size());

    for (const auto& node : nodes) {
        for (const auto& field : node.pointerFields) {
            auto nextAddress = mode == EMode::PRE ? field.preValue : field.postValue;
            auto next = GetNodeOrNull(nextAddress);
            if (!next || next != &target) continue;
            result.push_back(&field);
        }
    }

    return result;
}


//
// Flow Graph creation
//

inline FlowGraphNode* TryGetOrCreateNode(FlowGraph& graph, LazyValuationMap& eval, SymbolicFactory& factory,
                                         const SymbolicVariableDeclaration& nextAddress) {

    // find the memory resource referenced by nextAddress, abort if none exists
    auto* resource = eval.GetMemoryResourceOrNull(nextAddress);
    if (!resource) return nullptr;

    // search for an existing node at the given address, care for aliasing
    auto& nextAlias = resource->node->Decl();
    if (auto find = graph.GetNodeOrNull(nextAlias)) return find;

    // create a new node
    auto& flowType = resource->flow.get().type;
    FlowGraphNode result(nextAlias, resource->isLocal, resource->flow, factory, flowType);
    result.dataFields.reserve(resource->fieldToValue.size());
    result.pointerFields.reserve(resource->fieldToValue.size());
    for (const auto&[name, value] : resource->fieldToValue) {
        Field field(name, value->Type(), value->Decl());
        if (value->Sort() == Sort::PTR) {
            field.preAllOutflow = factory.GetUnusedFlowVariable(flowType);
            field.preGraphOutflow = factory.GetUnusedFlowVariable(flowType);
            field.postAllOutflow = factory.GetUnusedFlowVariable(flowType);
            field.postGraphOutflow = factory.GetUnusedFlowVariable(flowType);
            result.pointerFields.push_back(std::move(field));
        } else {
            result.dataFields.push_back(std::move(field));
        }
    }

    // add to footprint
    graph.nodes.push_back(std::move(result));
    return &graph.nodes.back();
}

inline std::set<const SymbolicVariableDeclaration*> ExpandGraph(FlowGraph& graph, LazyValuationMap& eval, SymbolicFactory& factory, std::size_t maxDepth) {
    if (maxDepth == 0) return { };
    std::set<std::pair<std::size_t, FlowGraphNode*>, std::greater<>> worklist;
    worklist.emplace(maxDepth, &*graph.nodes.begin());
    std::set<const SymbolicVariableDeclaration*> missing;

    auto handleAliases = [&](FlowGraphNode& node){
        auto getPointerValue = [&](const SymbolicVariableDeclaration& address) -> const SymbolicVariableDeclaration& {
            if (auto node = graph.GetNodeOrNull(address)) return node->address;
            if (auto memory = eval.GetMemoryResourceOrNull(address)) return memory->node->Decl();
            return address;
        };
        for (auto& field : node.pointerFields) {
            field.preValue = getPointerValue(field.preValue);
            field.postValue = getPointerValue(field.postValue);
        }
    };

    // expand until maxDepth is reached
    while (!worklist.empty()) {
        auto[depth, node] = *worklist.begin();
        worklist.erase(worklist.begin());
        handleAliases(*node);
        if (depth == 0) continue;

        bool publish = !node->postLocal;
        for (auto& field : node->pointerFields) {
            for (const auto& nextAddress : { field.preValue, field.postValue }) {
                if (auto nextNode = TryGetOrCreateNode(graph, eval, factory, nextAddress)) {
                    if (publish) nextNode->postLocal = false;
                    worklist.emplace(depth - 1, nextNode);
                } else {
                    missing.insert(&nextAddress.get());
                }
            }
        }
    }

    return missing;
}

inline std::unique_ptr<Annotation> ExtendStateForFootprintConstruction(FlowGraph& graph, SymbolicFactory& factory,
                                                                       const std::set<const SymbolicVariableDeclaration*>& missing) {
    EncodedFlowGraph encoding(std::move(graph));

    // derive stack knowledge
    auto candidates = CandidateGenerator::Generate(encoding.graph, EMode::PRE, CandidateGenerator::FlowLevel::FAST);
    z3::expr_vector expressions(encoding.context);
    for (const auto& candidate : candidates) expressions.push_back(encoding.encoder(*candidate));
    auto implied = engine::ComputeImplied(encoding.solver, expressions);
    for (std::size_t index = 0; index < implied.size(); ++index) {
        if (implied.at(index)) encoding.graph.pre->now->conjuncts.push_back(std::move(candidates.at(index)));
    }

    // expand memory
    auto force = missing;
    for (const auto& node : encoding.graph.nodes) force.insert(&node.address);
    encoding.graph.pre->now = engine::ExpandMemoryFrontier(std::move(encoding.graph.pre->now), factory, encoding.graph.config, missing, std::move(force), false);

    return std::move(encoding.graph.pre);
}

inline void CheckGraph(const FlowGraph& graph, const FlowGraphNode& root) {
    // cyclic flow graphs are not supported
    if (!graph.GetIncomingEdges(root, EMode::PRE).empty() || !graph.GetIncomingEdges(root, EMode::POST).empty()) {
        // TODO: support cycles
        //       - require unchanged flow (as if the edge closing a cycle was leading outside the footprint), or
        //       - require empty flow, or
        //       - require flow decrease (sufficient?)
        throw std::logic_error("Unsupported update: cyclic flow graphs are not supported");
    }
}

inline FlowGraph MakeFootprintGraph(std::unique_ptr<Annotation> state, const SymbolicVariableDeclaration& rootAddress, const SolverConfig& config,
                                    std::size_t depth, const std::function<void(FlowGraphNode&)>& applyRootUpdates) {
    std::set<const SymbolicVariableDeclaration*> missing;
    while (true) {
        assert(state);
        FlowGraph graph{ config, {}, std::move(state) };
        SymbolicFactory factory(*graph.pre);
        LazyValuationMap eval(*graph.pre);

        // make root node
        FlowGraphNode* root = TryGetOrCreateNode(graph, eval, factory, rootAddress);
        if (!root) throw std::logic_error("Cannot initialize flow graph."); // TODO: better error handling
        root->needed = true;
        root->preGraphInflow = root->preAllInflow;
        root->postGraphInflow = root->preAllInflow;
        root->postAllInflow = root->preAllInflow;
        applyRootUpdates(*root);

        // expand graph
        auto newMissing = ExpandGraph(graph, eval, factory, depth);
        if (missing == newMissing) {
            CheckGraph(graph, *root);
            return graph;
        }

        missing = std::move(newMissing);
        state = ExtendStateForFootprintConstruction(graph, factory, missing);
    }
}

FlowGraph engine::MakeFlowFootprint(std::unique_ptr<heal::Annotation> pre, const MultiUpdate& update, const SolverConfig& config) {
    static Timer timer("engine::MakeFlowFootprint");
    auto measurement = timer.Measure();

    heal::InlineAndSimplify(*pre);
    assert(!update.updates.empty());
    LazyValuationMap eval(*pre); // TODO: avoid duplication wrt. FinalizeFromRoot
    SymbolicFactory factory(*pre); // TODO: avoid duplication wrt. FinalizeFromRoot

    std::vector<std::pair<const SymbolicVariableDeclaration*, std::string>> lhsDereferences;
    std::vector<const SymbolicVariableDeclaration*> rhsValues;
    lhsDereferences.reserve(update.updates.size());
    rhsValues.reserve(update.updates.size());

    for (const auto& [lhs, rhs] : update) {
        // evaluate rhs
        if (auto var = dynamic_cast<const VariableExpression*>(rhs)) {
            rhsValues.push_back(&eval.GetValueOrFail(var->decl));
        } else {
            auto rhsValue = eval.Evaluate(*rhs);
            auto symbol = &factory.GetUnusedSymbolicVariable(rhs->type());
            auto axiom = std::make_unique<SymbolicAxiom>(std::move(rhsValue), SymbolicAxiom::EQ,
                                                         std::make_unique<SymbolicVariable>(*symbol));
            pre->now->conjuncts.push_back(std::move(axiom));
            rhsValues.push_back(symbol);
        }

        // evaluate updated node
        auto lhsNodeEval = eval.Evaluate(*lhs->expr);
        if (auto var = dynamic_cast<const SymbolicVariable*>(lhsNodeEval.get())) {
            lhsDereferences.emplace_back(&var->Decl(), lhs->fieldname);
        } else {
            throw std::logic_error("Cannot create footprint for malformed dereference."); // TODO: better error handling
        }
    }

    // get root
    assert(lhsDereferences.size() == rhsValues.size());
    assert(lhsDereferences.size() == update.updates.size());
    assert(!lhsDereferences.empty());
    auto root = lhsDereferences.front().first;
    auto depth = config.GetMaxFootprintDepth(lhsDereferences.front().second);

    // construct flow footprint
    auto result = MakeFootprintGraph(std::move(pre), *root, config, depth, [&lhsDereferences,&rhsValues,root](auto& rootNode) {
        for (std::size_t index = 0; index < lhsDereferences.size(); ++index) {
            auto [address, field] = lhsDereferences.at(index);
            if (address != root) continue;
            auto value = rhsValues.at(index);
            GetFieldOrFail(rootNode, field).postValue = *value;
        }
    });

    // postprocess: try apply non-root updates
    for (std::size_t index = 0; index < lhsDereferences.size(); ++index) {
        auto [address, field] = lhsDereferences.at(index);
        if (address == root) continue;
        auto value = rhsValues.at(index);
        auto node = result.GetNodeOrNull(*address);
        if (!node) throw std::logic_error("Cannot create footprint for parallel update");
        GetFieldOrFail(*node, field).postValue = *value;
        node->needed = true;
    }

    // begin debug
    std::cout << "Footprint:" << std::endl;
    for (const auto& node : result.nodes) {
        std::cout << "   - Node " << node.address.name << std::endl;
        for (const auto& next : node.pointerFields) {
            std::cout << "      - " << node.address.name << "->" << next.name << " == " << next.preValue.get().name << " / " << next.postValue.get().name << std::endl;
        }
    }
    // end debug

    return result;
}

FlowGraph engine::MakePureHeapGraph(std::unique_ptr<heal::Annotation> state, const SolverConfig& config) {
    static Timer timer("engine::MakePureHeapGraph");
    auto measurement = timer.Measure();

    heal::InlineAndSimplify(*state);
//    std::cout << "Making pure heap graph for: " << *state << std::endl;
    FlowGraph graph{config, {}, std::move(state)};

    SymbolicFactory factory(*graph.pre);
    LazyValuationMap eval(*graph.pre);

    // add all nodes
    auto memory = heal::CollectMemory(*graph.pre);
    for (const auto* mem : memory) {
        TryGetOrCreateNode(graph, eval, factory, mem->node->Decl());
    }

    // make pre/post state equal
    for (auto& node : graph.nodes) {
        node.needed = true;
        node.postAllInflow = node.preAllInflow;
        node.postKeyset = node.preKeyset;
        node.postGraphInflow = node.preGraphInflow;

        for (auto& field : node.pointerFields) {
            field.postAllOutflow = field.preAllOutflow;
            field.postGraphOutflow = field.preGraphOutflow;
        }
    }

    // begin debug
//    std::cout << "Pure heap graph:" << std::endl;
//    for (const auto& node : graph.nodes) {
//        std::cout << "   - Node " << node.address.name << std::endl;
//        for (const auto& next : node.pointerFields) {
//            std::cout << "      - " << node.address.name << "->" << next.name << " == " << next.preValue.get().name << " / " << next.postValue.get().name << std::endl;
//        }
//    }
//    std::cout << "graph.pre: " << *graph.pre << std::endl;
    // end debug

    // TODO: find a root (some node that is not reachable from any other node)?
    return graph;
}


//
// Encode flow graph
//

inline z3::expr EncodeNodeLocality(const FlowGraphNode& node, EncodedFlowGraph& encoding) {
    if (!node.preLocal) return encoding.context.bool_val(true);
    auto& encoder = encoding.encoder;
    z3::expr_vector result(encoding.context);
    for (const auto& shared : encoding.graph.nodes) {
        if (shared.preLocal) continue;
        assert(&node != &shared);
        // shared pointers cannot point to node ones
        result.push_back(encoder(node.address) != encoder(shared.address));
        for (const auto& field : shared.pointerFields) {
            result.push_back(encoder(node.address) != encoder(field.preValue));
        }
    }
    for (const auto& shared : heal::CollectVariables(*encoding.graph.pre)) {
        if (!shared->variable->decl.is_shared) continue;
        result.push_back(encoder(node.address) != encoder(*shared->value));
    }
    return z3::mk_and(result);
}

inline z3::expr EncodeAcyclicity(const FlowGraphNode& node, EncodedFlowGraph& encoding) {
    z3::expr_vector result(encoding.context);
    for (const auto& field : node.pointerFields) {
        result.push_back(encoding.encoder(node.address) != encoding.encoder(field.preValue));
    }
    return z3::mk_and(result);
}

z3::expr EncodedFlowGraph::EncodeNodeInvariant(const FlowGraphNode& node, EMode mode) {
    auto invariant = graph.config.GetNodeInvariant(*node.ToLogic(mode));
    return encoder(*invariant);
}

template<typename T>
inline z3::expr EncodePredicate(EncodedFlowGraph& encoding, const FlowGraphNode& node, const z3::expr& value, EMode mode, T getPredicate) {
    auto memory = node.ToLogic(mode);
    auto& dummyArg = SymbolicFactory(*memory).GetUnusedSymbolicVariable(encoding.graph.config.GetFlowValueType());
    auto predicate = getPredicate(*memory, dummyArg);

    z3::expr_vector replace(encoding.encoder.context), with(encoding.encoder.context);
    replace.push_back(encoding.encoder(dummyArg));
    with.push_back(value);
    return encoding.encoder(*predicate).substitute(replace, with);
}

z3::expr EncodedFlowGraph::EncodeNodeOutflow(const FlowGraphNode& node, const std::string& fieldName, const z3::expr& value, EMode mode) {
    return EncodePredicate(*this, node, value, mode, [this, &fieldName](auto& memory, auto& arg) {
        return graph.config.GetOutflowContains(memory, fieldName, arg);
    });
}

z3::expr EncodedFlowGraph::EncodeNodeContains(const FlowGraphNode& node, const z3::expr& value, EMode mode) {
    return EncodePredicate(*this, node, value, mode, [this](auto& memory, auto& arg) {
        return graph.config.GetLogicallyContains(memory, arg);
    });
}

z3::expr EncodeOutflow(EncodedFlowGraph& encoding, const FlowGraphNode& node, const Field& field, EMode mode) {
    /* Remark:
     * Laminar flow may suggest that the keyset due to frame flow needs not be considered.
     * However, we need to check that node inflow remains unique after update. Ignoring inflow that is due to frame
     * flow on a predecessor might result in erroneous conclusions. Hence, we track root and all outflow.
     * One may want to check whether this is strictly necessary!
     */
    auto& graph = encoding.graph;
    auto& encoder = encoding.encoder;

    bool forPre = mode == EMode::PRE;
    auto qv = encoder.QuantifiedVariable(graph.config.GetFlowValueType().sort);
    auto mkFlow = [&](auto value) { return encoding.EncodeNodeOutflow(node, field.name, value, mode); };
    auto graphIn = encoder(forPre ? node.preGraphInflow : node.postGraphInflow);
    auto graphOut = encoder(forPre ? field.preGraphOutflow.value() : field.postGraphOutflow.value());
    auto allIn = encoder(forPre ? node.preAllInflow : node.postAllInflow);
    auto allOut = encoder(forPre ? field.preAllOutflow.value() : field.postAllOutflow.value());
    z3::expr_vector result(encoder.context);

    // outflow
    result.push_back(z3::forall(qv, (encoder.MakeFlowContains(graphIn, qv) && mkFlow(qv)) == encoder.MakeFlowContains(graphOut, qv)));
    result.push_back(z3::forall(qv, (encoder.MakeFlowContains(allIn, qv) && mkFlow(qv)) == encoder.MakeFlowContains(allOut, qv)));

    // outflow received by successor
    auto* successor = graph.GetNodeOrNull(forPre ? field.preValue : field.postValue);
    if (successor) {
        auto nextGraphIn = encoder(forPre ? successor->preGraphInflow : successor->postGraphInflow);
        auto nextAllIn = encoder(forPre ? successor->preAllInflow : successor->postAllInflow);
        result.push_back(z3::forall(qv, z3::implies(encoder.MakeFlowContains(graphOut, qv), encoder.MakeFlowContains(nextGraphIn, qv))));
        result.push_back(z3::forall(qv, z3::implies(encoder.MakeFlowContains(allOut, qv), encoder.MakeFlowContains(nextAllIn, qv))));
    }

    return z3::mk_and(result);
}

inline std::vector<const SymbolicFlowDeclaration*> GetGraphInflow(const FlowGraph& graph, const FlowGraphNode& node, EMode mode) {
    auto incomingEdges = graph.GetIncomingEdges(node, mode);
    bool forPre = mode == EMode::PRE;
    std::vector<const SymbolicFlowDeclaration*> result;
    result.reserve(incomingEdges.size());
    for (auto* field : incomingEdges) {
        result.push_back(forPre ? &field->preGraphOutflow.value().get() : &field->postGraphOutflow.value().get());
    }
    return result;
}

inline z3::expr EncodeFlowRules(EncodedFlowGraph& encoding, const FlowGraphNode& node) {
    auto& encoder = encoding.encoder;
    auto qv = encoder.QuantifiedVariable(node.frameInflow.get().type.sort);
    z3::expr_vector result(encoder.context);
    auto addRule = [&qv, &result](auto pre, auto imp) { result.push_back(z3::forall(qv, z3::implies(pre, imp))); };

    z3::expr_vector preOuts(encoder.context);
    z3::expr_vector postOuts(encoder.context);
    for (const auto& field : node.pointerFields) {
        preOuts.push_back(encoder.MakeFlowContains(field.preAllOutflow.value(), qv));
        postOuts.push_back(encoder.MakeFlowContains(field.postAllOutflow.value(), qv));
    }

    auto preGraph = encoder.MakeFlowContains(node.preGraphInflow, qv);
    auto postGraph = encoder.MakeFlowContains(node.postGraphInflow, qv);
    auto preAll = encoder.MakeFlowContains(node.preAllInflow, qv);
    auto postAll = encoder.MakeFlowContains(node.postAllInflow, qv);
    auto preKey = encoder.MakeFlowContains(node.preKeyset, qv);
    auto postKey = encoder.MakeFlowContains(node.postKeyset, qv);
    auto frame = encoder.MakeFlowContains(node.frameInflow, qv);
    auto preOut = z3::mk_or(preOuts);
    auto postOut = z3::mk_or(postOuts);

    // root flow is subset of all flow
    addRule(preGraph, preAll);
    addRule(postGraph, postAll);

    // frame flow is subset of all flow
    addRule(frame, preAll);
    addRule(frame, postAll);

    // all flow is either frame flow or root flow
    addRule(preAll, preGraph || frame);
    addRule(postAll, postGraph || frame);

    // redundant
    addRule(preGraph == postGraph, preAll == postAll);

    // keyset is subset of all flow and not outflow
    addRule(preKey, preAll && !preOut);
    addRule(postKey, postAll && !postOut);

    // keyset is inflow minus outflow
    addRule(preAll && !preOut, preKey);
    addRule(postAll && !postOut, postKey);

    // graphInflow is due to predecessors (skip if graph flow is equal to all flow)
    if (&node.preAllInflow.get() != &node.preGraphInflow.get() && &node.postAllInflow.get() != &node.postGraphInflow.get()) {
        for (auto[theRoot, mode] : {std::make_pair(preGraph, EMode::PRE), std::make_pair(postGraph, EMode::POST)}) {
            auto incomingRootFlow = GetGraphInflow(encoding.graph, node, mode);
            z3::expr_vector inflow(encoder.context);
            for (auto* edge : incomingRootFlow) inflow.push_back(encoder.MakeFlowContains(*edge, qv));
            addRule(theRoot, z3::mk_or(inflow));
        }
    }

    return mk_and(result);
}

z3::expr EncodedFlowGraph::EncodeKeysetDisjointness(EMode mode) {
    auto qv = encoder.QuantifiedVariable(graph.config.GetFlowValueType().sort);
    z3::expr_vector keyset(encoder.context);
    for (const auto& node : graph.nodes) {
        auto& nodeKeyset = mode == EMode::PRE ? node.preKeyset.get() : node.postKeyset.get();
        keyset.push_back(encoder.MakeFlowContains(nodeKeyset, qv));
    }
    return z3::forall(qv, z3::atmost(keyset, 1));
}

z3::expr EncodedFlowGraph::EncodeInflowUniqueness(const FlowGraphNode& node, EMode mode) {
    auto isNonEmpty = [&](const SymbolicFlowDeclaration& flow){
        InflowEmptinessAxiom empty(flow, false);
        return encoder(empty);
    };

    z3::expr_vector inflow(encoder.context);
    inflow.push_back(isNonEmpty(node.frameInflow));
    for (const auto* edge : GetGraphInflow(graph, node, mode)) {
        inflow.push_back(isNonEmpty(*edge));
    }

    return z3::atmost(inflow, MAX_INFLOW_PREDECESSORS);
}

inline z3::expr EncodeFlow(EncodedFlowGraph& encoding) {
    auto& graph = encoding.graph;
    auto& encoder = encoding.encoder;

    z3::expr_vector result(encoder.context);
    result.push_back(encoder(*graph.pre));
    auto qv = encoder.QuantifiedVariable(graph.config.GetFlowValueType().sort);

    // same inflow in root before and after update
//    auto& root = graph.nodes.front();
//    assert(&root.preAllInflow.get() == &root.postAllInflow.get());
//     TODO: if preGraph == preAll, do we need to explicitly tell the engine that the frame flow is empty?
//    result.push_back(z3::forall(qv, !encoder(root.frameInflow)(qv)));

    // go over graph and encode flow
    for (const auto& node : graph.nodes) {
        // node invariant
        result.push_back(encoding.EncodeNodeInvariant(node, EMode::PRE));
        result.push_back(encoding.EncodeInflowUniqueness(node, EMode::PRE));
        result.push_back(EncodeNodeLocality(node, encoding));
        result.push_back(EncodeAcyclicity(node, encoding));

        // general flow constraints
        result.push_back(EncodeFlowRules(encoding, node));

        // outflow
        for (const auto& field : node.pointerFields) {
            result.push_back(EncodeOutflow(encoding, node, field, EMode::PRE));
            result.push_back(EncodeOutflow(encoding, node, field, EMode::POST));
        }
    }

    for (const auto* resource : heal::CollectVariables(*encoding.graph.pre)) {
        if (!resource->variable->decl.is_shared) continue;
        result.push_back(encoder(*encoding.graph.config.GetSharedVariableInvariant(*resource)));
    }

    // add assumptions for pre flow: keyset disjointness, inflow uniqueness
    result.push_back(encoding.EncodeKeysetDisjointness(EMode::PRE));

    return z3::mk_and(result);
}

EncodedFlowGraph::EncodedFlowGraph(FlowGraph&& graph_) : solver(context), encoder(context, solver), graph(std::move(graph_)) {
    static Timer timer("EncodedFlowGraph::EncodedFlowGraph");
    auto measurement = timer.Measure();

//    assert(!graph_.nodes.empty());
    solver.add(EncodeFlow(*this));
//    assert(engine.check() != z3::unsat);
}