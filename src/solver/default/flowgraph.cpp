#include "flowgraph.hpp"

#include "eval.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

//
// Flow graph access
//

const FlowGraphNode* FlowGraph::GetNodeOrNull(const heal::SymbolicVariableDeclaration &address) const {
    // search for an existing node at the given address
    auto pred = [&address](const auto &item) { return &item.address == &address; };
    auto find = std::find_if(nodes.begin(), nodes.end(), pred);
    if (find != nodes.end()) return &(*find);
    return nullptr;
}

FlowGraphNode* FlowGraph::GetNodeOrNull(const heal::SymbolicVariableDeclaration &address) {
    return const_cast<FlowGraphNode *>(std::as_const(*this).GetNodeOrNull(address));
}

std::unique_ptr<heal::PointsToAxiom> FlowGraphNode::ToLogic(EMode mode) const {
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto &field : pointerFields) {
        fields[field.name] = std::make_unique<SymbolicVariable>(mode == EMode::PRE ? field.preValue : field.postValue);
    }
    for (const auto &field : dataFields) {
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
    for (auto &field : node.pointerFields) {
        if (field.name == name) return field;
    }
    for (auto &field : node.dataFields) {
        if (field.name == name) return field;
    }
    throw std::logic_error("Could not find field."); // TODO: better error handling
}

inline bool IsRoot(const FlowGraph &graph, const FlowGraphNode &node) {
    return &node == &graph.nodes.front();
}

std::vector<const Field*> FlowGraph::GetIncomingEdges(const FlowGraphNode& target, EMode mode) const {
    std::vector<const Field*> result;
    result.reserve(nodes.size() * target.pointerFields.size());

    for (const auto &node : nodes) {
        for (const auto &field : node.pointerFields) {
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

inline FlowGraphNode* TryGetOrCreateNode(FlowGraph &graph, LazyValuationMap &eval, SymbolicFactory &factory,
                                         const SymbolicVariableDeclaration &nextAddress) {

    // find the memory resource referenced by nextAddress, abort if none exists
    auto *resource = eval.GetMemoryResourceOrNull(nextAddress);
    if (!resource) return nullptr;

    // search for an existing node at the given address, care for aliasing
    auto &nextAlias = resource->node->Decl();
    if (auto find = graph.GetNodeOrNull(nextAlias)) return find;

    // create a new node
    auto &flowType = resource->flow.get().type;
    auto &preRootInflow = factory.GetUnusedFlowVariable(flowType); // compute later
    auto &postRootInflow = factory.GetUnusedFlowVariable(flowType); // compute later
    auto &postAllInflow = factory.GetUnusedFlowVariable(flowType); // flow might have changed
    auto &preKeyset = factory.GetUnusedFlowVariable(flowType); // compute later
    auto &postKeyset = factory.GetUnusedFlowVariable(flowType); // compute later
    auto &frameInflow = factory.GetUnusedFlowVariable(flowType); // compute later
    FlowGraphNode result{nextAlias, false, resource->isLocal, resource->isLocal, resource->flow, preRootInflow,
                         preKeyset, postAllInflow, postRootInflow, postKeyset, frameInflow, {}, {}};
    result.dataFields.reserve(resource->fieldToValue.size());
    result.pointerFields.reserve(resource->fieldToValue.size());
    for (const auto&[name, value] : resource->fieldToValue) {
        Field field{name, value->Type(), value->Decl(), value->Decl(), std::nullopt, std::nullopt, std::nullopt,
                    std::nullopt};
        if (value->Sort() == Sort::PTR) {
            field.preAllOutflow = factory.GetUnusedFlowVariable(flowType); // compute later
            field.preRootOutflow = factory.GetUnusedFlowVariable(flowType); // compute later
            field.postAllOutflow = factory.GetUnusedFlowVariable(flowType); // compute later
            field.postRootOutflow = factory.GetUnusedFlowVariable(flowType); // compute later
            result.pointerFields.push_back(std::move(field));
        } else {
            result.dataFields.push_back(std::move(field));
        }
    }

    // add to footprint
    graph.nodes.push_back(std::move(result));
    return &graph.nodes.back();
}

inline void ExpandGraph(FlowGraph &graph, LazyValuationMap &eval, SymbolicFactory &factory, std::size_t maxDepth) {
    if (maxDepth == 0) return;
    std::set<std::pair<std::size_t, FlowGraphNode *>, std::greater<>> worklist;
    worklist.emplace(maxDepth, &*graph.nodes.begin());

    while (!worklist.empty()) {
        auto[depth, node] = *worklist.begin();
        worklist.erase(worklist.begin());
        if (depth == 0) continue;

        for (auto &field : node->pointerFields) {
            for (auto nextAddress : {&field.preValue, &field.postValue}) {
                auto *nextNode = TryGetOrCreateNode(graph, eval, factory, *nextAddress);
                if (!nextNode) continue;
                *nextAddress = nextNode->address; // inline alias
                if (!node->postLocal) nextNode->postLocal = false; // publish node
                worklist.emplace(depth - 1, nextNode);
            }
        }
    }
}

inline void FinalizeFlowGraph(FlowGraph& graph, const SymbolicVariableDeclaration& rootAddress, std::size_t depth,
                              std::optional<std::function<void(FlowGraphNode&)>>&& applyRootUpdate=std::nullopt) {

    SymbolicFactory factory(*graph.pre);
    LazyValuationMap eval(*graph.pre);

    // make root node
    FlowGraphNode *root = TryGetOrCreateNode(graph, eval, factory, rootAddress);
    if (!root) throw std::logic_error("Cannot initialize flow graph."); // TODO: better error handling
    root->needed = true;
    root->preRootInflow = root->preAllInflow;
    root->postRootInflow = root->preAllInflow;
    root->postAllInflow = root->preAllInflow;
    if (applyRootUpdate) applyRootUpdate.value()(*root);

    // generate flow graph
    ExpandGraph(graph, eval, factory, depth);

    // cyclic flow graphs are not supported
    if (!graph.GetIncomingEdges(*root, EMode::PRE).empty() || !graph.GetIncomingEdges(*root, EMode::POST).empty()) {
        // TODO: support cycles
        //       - require unchanged flow (as if the edge closing a cycle was leading outside the footprint), or
        //       - require empty flow, or
        //       - require flow decrease (sufficient?)
        throw std::logic_error("Unsupported update: cyclic flow graphs are not supported");
    }
}

FlowGraph solver::MakeFlowGraph(std::unique_ptr<Annotation> state, const SymbolicVariableDeclaration& rootAddress, const SolverConfig& config, std::size_t depth) {
    FlowGraph graph{config, {}, std::move(state)};
    FinalizeFlowGraph(graph, rootAddress, depth);
    return graph;
}

FlowGraph solver::MakeFlowFootprint(std::unique_ptr<heal::Annotation> pre, const cola::Dereference &lhs,
                                    const cola::SimpleExpression &rhs, const SolverConfig &config) {

    FlowGraph graph{config, {}, std::move(pre)};

    // evaluate rhs
    EagerValuationMap eval(*graph.pre); // TODO: avoid duplication wrt. FinalizeFlowGraph
    SymbolicFactory factory(*graph.pre); // TODO: avoid duplication wrt. FinalizeFlowGraph
    const SymbolicVariableDeclaration* rhsEval = nullptr;
    if (auto var = dynamic_cast<const VariableExpression *>(&rhs)) {
        rhsEval = &eval.GetValueOrFail(var->decl);
    } else {
        auto rhsValue = eval.Evaluate(rhs);
        rhsEval = &factory.GetUnusedSymbolicVariable(rhs.type());
        auto axiom = std::make_unique<SymbolicAxiom>(std::move(rhsValue), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(*rhsEval));
        graph.pre->now->conjuncts.push_back(std::move(axiom));
    }
    assert(rhsEval);

    // evaluate updated node
    auto lhsNodeEval = eval.Evaluate(*lhs.expr);
    const SymbolicVariableDeclaration* root = nullptr;
    if (auto var = dynamic_cast<const SymbolicVariable*>(lhsNodeEval.get())) {
        root = &var->Decl();
    } else {
        throw std::logic_error("Cannot create footprint for malformed dereference."); // TODO: better error handling
    }
    assert(root);

    // construct flow footprint
    FinalizeFlowGraph(graph, *root, config.maxFootprintDepth, [&lhs,&rhsEval](auto& rootNode){
        GetFieldOrFail(rootNode, lhs.fieldname).postValue = *rhsEval;
    });
    return graph;
}


//
// Encode flow graph
//

z3::expr EncodedFlowGraph::EncodeNodeInvariant(const FlowGraphNode& node, EMode mode) {
    if (!graph.config.applyInvariantToLocal && mode == EMode::PRE ? node.preLocal : node.postLocal)
        return encoder.context.bool_val(true);

    auto invariant = graph.config.invariant->Instantiate(*node.ToLogic(mode)); // TODO: z3 substitution could do this
    return encoder(*invariant);
}

z3::expr EncodedFlowGraph::EncodeNodePredicate(const Predicate& predicate, const FlowGraphNode& node, const z3::expr& value, EMode mode) {
    auto &arg = predicate.vars[0];
    auto instance = predicate.Instantiate(*node.ToLogic(mode), arg); // TODO: z3 substitution could do this
    z3::expr_vector replace(encoder.context), with(encoder.context);
    replace.push_back(encoder(arg));
    with.push_back(value);
    return encoder(*instance).substitute(replace, with);
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
    auto qv = encoder.QuantifiedVariable(graph.config.flowDomain->GetFlowValueType().sort);
    auto &flowPredicate = graph.config.flowDomain->GetOutFlowContains(field.name);
    auto mkFlow = [&](auto value) { return encoding.EncodeNodePredicate(flowPredicate, node, value, mode); };
    auto rootIn = encoder(forPre ? node.preRootInflow : node.postRootInflow);
    auto rootOut = encoder(forPre ? field.preRootOutflow.value() : field.postRootOutflow.value());
    auto allIn = encoder(forPre ? node.preAllInflow : node.postAllInflow);
    auto allOut = encoder(forPre ? field.preAllOutflow.value() : field.postAllOutflow.value());
    z3::expr_vector result(encoder.context);

    // outflow
    result.push_back(z3::forall(qv, (rootIn(qv) && mkFlow(qv)) == rootOut(qv)));
    result.push_back(z3::forall(qv, (allIn(qv) && mkFlow(qv)) == allOut(qv)));

    // outflow received by successor
    auto *successor = graph.GetNodeOrNull(forPre ? field.preValue : field.postValue);
    if (successor) {
        auto nextRootIn = encoder(forPre ? successor->preRootInflow : successor->postRootInflow);
        auto nextAllIn = encoder(forPre ? successor->preAllInflow : successor->postAllInflow);
        result.push_back(z3::forall(qv, z3::implies(rootOut(qv), nextRootIn(qv))));
        result.push_back(z3::forall(qv, z3::implies(allOut(qv), nextAllIn(qv))));
    }

    return z3::mk_and(result);
}

inline std::vector<const SymbolicFlowDeclaration*> GetRootInflowFromPredecessors(const FlowGraph& graph, const FlowGraphNode& node, EMode mode) {
    assert(!IsRoot(graph, node));
    auto incomingEdges = graph.GetIncomingEdges(node, mode);
    bool forPre = mode == EMode::PRE;
    std::vector<const SymbolicFlowDeclaration *> result;
    result.reserve(incomingEdges.size());
    for (auto *field : incomingEdges) {
        result.push_back(forPre ? &field->preRootOutflow.value().get() : &field->postRootOutflow.value().get());
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
    for (const auto &field : node.pointerFields) {
        preOuts.push_back(encoder(field.preAllOutflow.value())(qv));
        postOuts.push_back(encoder(field.postAllOutflow.value())(qv));
    }

    auto preRoot = encoder(node.preRootInflow)(qv);
    auto postRoot = encoder(node.postRootInflow)(qv);
    auto preAll = encoder(node.preAllInflow)(qv);
    auto postAll = encoder(node.postAllInflow)(qv);
    auto preKey = encoder(node.preKeyset)(qv);
    auto postKey = encoder(node.postKeyset)(qv);
    auto frame = encoder(node.frameInflow)(qv);
    auto preOut = z3::mk_or(preOuts);
    auto postOut = z3::mk_or(postOuts);

    // root flow is subset of all flow
    addRule(preRoot, preAll);
    addRule(postRoot, postAll);

    // frame flow is subset of all flow
    addRule(frame, preAll);
    addRule(frame, postAll);

    // all flow is either frame flow or root flow
    addRule(preAll, preRoot || frame);
    addRule(postAll, postRoot || frame);

    // redundant
    addRule(preRoot == postRoot, preAll == postAll);

    // keyset is subset of all flow and not outflow
    addRule(preKey, preAll && !preOut);
    addRule(postKey, postAll && !postOut);

    // keyset is inflow minus outflow
    addRule(preAll && !preOut, preKey);
    addRule(postAll && !postOut, postKey);

    // rootInflow is due to predecessors (except in root node)
    if (!IsRoot(encoding.graph, node)) {
        for (auto[theRoot, mode] : {std::make_pair(preRoot, EMode::PRE), std::make_pair(postRoot, EMode::POST)}) {
            auto incomingRootFlow = GetRootInflowFromPredecessors(encoding.graph, node, mode);
            z3::expr_vector inflow(encoder.context);
            for (auto* edge : incomingRootFlow) inflow.push_back(encoder(*edge)(qv));
            addRule(theRoot, z3::mk_or(inflow));
        }
    }

    return mk_and(result);
}

z3::expr EncodedFlowGraph::EncodeKeysetDisjointness(EMode mode) {
    auto qv = encoder.QuantifiedVariable(graph.config.flowDomain->GetFlowValueType().sort);
    z3::expr_vector keyset(encoder.context);
    for (const auto &node : graph.nodes) {
        auto &nodeKeyset = mode == EMode::PRE ? node.preKeyset.get() : node.postKeyset.get();
        keyset.push_back(encoder(nodeKeyset)(qv));
    }
    return z3::forall(qv, z3::atmost(keyset, 1));
}

z3::expr EncodedFlowGraph::EncodeInflowUniqueness(EMode mode) {
    // uniqueness
    auto qv = encoder.QuantifiedVariable(graph.config.flowDomain->GetFlowValueType().sort);
    z3::expr_vector result(encoder.context);

    for (const auto &node : graph.nodes) {
        if (IsRoot(graph, node)) continue;
        z3::expr_vector inflow(encoder.context);
        inflow.push_back(encoder(node.frameInflow)(qv));
        for (auto* edge : GetRootInflowFromPredecessors(graph, node, mode)) {
            inflow.push_back(encoder(*edge)(qv));
        }
        result.push_back(z3::forall(qv, z3::atmost(inflow, 1)));
    }

    return z3::mk_and(result);
}

inline z3::expr EncodeFlow(EncodedFlowGraph& encoding) {
    auto& graph = encoding.graph;
    auto& encoder = encoding.encoder;

    z3::expr_vector result(encoder.context);
    result.push_back(encoder(*graph.pre));
    auto qv = encoder.QuantifiedVariable(graph.config.flowDomain->GetFlowValueType().sort);

    // same inflow in root before and after update
    auto &root = graph.nodes.front();
    assert(&root.preAllInflow.get() == &root.preRootInflow.get());
    assert(&root.preAllInflow.get() == &root.postRootInflow.get());
    assert(&root.preAllInflow.get() == &root.postAllInflow.get());
    result.push_back(z3::forall(qv, !encoder(root.frameInflow)(qv)));

    // go over graph and encode flow
    for (const auto &node : graph.nodes) {
        // node invariant
        result.push_back(encoding.EncodeNodeInvariant(node, EMode::PRE));

        // general flow constraints
        result.push_back(EncodeFlowRules(encoding, node));

        // outflow
        for (const auto &field : node.pointerFields) {
            result.push_back(EncodeOutflow(encoding, node, field, EMode::PRE));
            result.push_back(EncodeOutflow(encoding, node, field, EMode::POST));
        }
    }

    // add assumptions for pre flow: keyset disjointness, inflow uniqueness
    result.push_back(encoding.EncodeKeysetDisjointness(EMode::PRE));
    result.push_back(encoding.EncodeInflowUniqueness(EMode::PRE));

    return z3::mk_and(result);
}

EncodedFlowGraph::EncodedFlowGraph(FlowGraph&& graph_) : solver(context), encoder(context, solver), graph(std::move(graph_)) {
    solver.add(EncodeFlow(*this));
//    assert(solver.check() != z3::unsat);
}