#include "engine/encoding.hpp"

#include "logics/util.hpp"

using namespace plankton;


inline std::vector<const SymbolDeclaration*> GetGraphInflow(const FlowGraph& graph,
                                                            const FlowGraphNode& node, EMode mode) {
    auto incomingEdges = graph.GetIncomingEdges(node, mode);
    auto result = plankton::MakeVector<const SymbolDeclaration*>(incomingEdges.size());
    for (auto* field : incomingEdges) result.push_back(&field->GraphOutflow(mode));
    return result;
}

z3::expr
Encoding::EncodeOutflow(const FlowGraphNode& node, const PointerField& field, EMode mode) {
    /* Remark:
     * Laminar flow may suggest that the keyset due to frame flow needs not be considered.
     * However, we need to check that the node inflow remains unique after update. Ignoring inflow that is due to
     * the frame flow on a predecessor might result in erroneous conclusions. Hence, we track root and all outflow.
     * // TODO: check whether this is strictly necessary
     */
    z3::expr_vector result(context);
    auto& graph = node.parent;
    auto flowSort = graph.config.GetFlowValueType().sort;
    auto addRule = [this,&result,flowSort](const auto& func){
        result.push_back(EncodeForAll(flowSort, func).AsExpr());
    };
    auto elemOfOutflow = [&](const EExpr& value) -> EExpr {
        return EExpr(EncodeOutflowContains(node, field.name, value, mode));
    };
    
    // outflow
    auto graphIn = Encode(node.GraphInflow(mode));
    auto graphOut = Encode(field.GraphOutflow(mode));
    auto allIn = Encode(node.AllInflow(mode));
    auto allOut = Encode(field.AllOutflow(mode));
    addRule([&](auto qv){ return (graphIn(qv) && elemOfOutflow(qv)) == graphOut(qv); });
    addRule([&](auto qv){ return (allIn(qv) && elemOfOutflow(qv)) == allOut(qv); });

    // outflow received by successor
    auto* successor = graph.GetNodeOrNull(field.Value(mode));
    if (successor) {
        auto nextGraphIn = Encode(successor->GraphInflow(mode));
        auto nextAllIn = Encode(successor->AllInflow(mode));
        addRule([&](auto qv){ return graphOut(qv) >> nextGraphIn(qv); });
        addRule([&](auto qv){ return allOut(qv) >> nextAllIn(qv); });
    }

    return z3::mk_and(result);
}

z3::expr Encoding::EncodeFlowRules(const FlowGraphNode& node) {
    z3::expr_vector result(context);
    auto qv = EExpr(MakeQuantifiedVariable(node.parent.config.GetFlowValueType().sort));
    auto addRule = [&qv, &result](const z3::expr& pre, const z3::expr& imp) {
        result.push_back(z3::forall(qv.AsExpr(), z3::implies(pre, imp)));
    };

    z3::expr_vector preOuts(context);
    z3::expr_vector postOuts(context);
    for (const auto& field : node.pointerFields) {
        preOuts.push_back(Encode(field.preAllOutflow)(qv).AsExpr());
        postOuts.push_back(Encode(field.postAllOutflow)(qv).AsExpr());
    }

    auto preGraph = Encode(node.preGraphInflow)(qv).AsExpr();
    auto postGraph = Encode(node.postGraphInflow)(qv).AsExpr();
    auto preAll = Encode(node.preAllInflow)(qv).AsExpr();
    auto postAll = Encode(node.postAllInflow)(qv).AsExpr();
    auto preKey = Encode(node.preKeyset)(qv).AsExpr();
    auto postKey = Encode(node.postKeyset)(qv).AsExpr();
    auto frame = Encode(node.frameInflow)(qv).AsExpr();
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
    if (&node.preAllInflow.get() != &node.preGraphInflow.get() &&
        &node.postAllInflow.get() != &node.postGraphInflow.get()) {
        auto makeInflow = [&,this](EMode mode){
            auto graphInflow = GetGraphInflow(node.parent, node, mode);
            z3::expr_vector inflow(context);
            for (auto* edge : graphInflow) inflow.push_back(Encode(*edge)(qv).AsExpr());
            return z3::mk_or(inflow);
        };
        addRule(preGraph, makeInflow(EMode::PRE));
        addRule(postGraph, makeInflow(EMode::POST));
    }

    return z3::mk_and(result);
}

EExpr Encoding::EncodeKeysetDisjointness(const FlowGraph& graph, EMode mode) {
    return EncodeForAll(graph.config.GetFlowValueType().sort, [this, &graph, mode](auto qv){
        auto keysets = plankton::MakeVector<EExpr>(graph.nodes.size());
        for (const auto& node : graph.nodes) keysets.push_back(Encode(node.Keyset(mode))(qv));
        return MakeAtMost(keysets, 1);
    });
}

EExpr Encoding::EncodeInflowUniqueness(const FlowGraph& graph, EMode mode) {
    auto isNonEmpty = [&](const SymbolDeclaration& flow) {
        return Encode(InflowEmptinessAxiom(flow, false));
    };

    auto result = plankton::MakeVector<EExpr>(graph.nodes.size());
    for (const auto& node : graph.nodes) {
        auto inflow = plankton::MakeVector<EExpr>(graph.nodes.size() + 1);
        for (const auto* edge : GetGraphInflow(graph, node, mode)) {
            inflow.push_back(isNonEmpty(*edge));
        }
        result.push_back(MakeAtMost(result, 1));
    }
    return MakeAnd(result);
}

EExpr Encoding::Encode(const FlowGraph& graph) {
    if (graph.nodes.empty()) return Bool(true);
    auto result = plankton::MakeVector<EExpr>(8 + graph.nodes.size() * graph.nodes.front().pointerFields.size());
    
    result.push_back(Encode(*graph.pre->now));
    result.push_back(EncodeInvariants(*graph.pre->now, graph.config));
    result.push_back(EncodeAcyclicity(*graph.pre->now));
    result.push_back(EncodeOwnership(*graph.pre->now));
    result.push_back(EncodeKeysetDisjointness(graph, EMode::PRE));
    result.push_back(EncodeInflowUniqueness(graph, EMode::PRE));
    
    auto qv = MakeQuantifiedVariable(graph.config.GetFlowValueType().sort);
    for (const auto& node : graph.nodes) {
        result.push_back(EExpr(EncodeFlowRules(node)));
        for (const auto& field : node.pointerFields) {
            result.push_back(EExpr(EncodeOutflow(node, field, EMode::PRE)));
            result.push_back(EExpr(EncodeOutflow(node, field, EMode::POST)));
        }
    }
    
    return MakeAnd(result);
}

inline std::pair<std::unique_ptr<MemoryAxiom>, const SymbolDeclaration&> MakeDummy(const FlowGraphNode& node, EMode mode) {
    auto axiom = node.ToLogic(mode);
    auto& symbol = SymbolFactory(*axiom).GetFreshFO(node.parent.config.GetFlowValueType());
    return { std::move(axiom), symbol };
}

EExpr Encoding::EncodeLogicallyContains(const FlowGraphNode& node, const EExpr& value, EMode mode) {
    auto [axiom, dummy] = MakeDummy(node, mode);
    auto predicate = node.parent.config.GetLogicallyContains(*axiom, dummy);
    return EExpr(Replace(Encode(*predicate), Encode(dummy), value));
}

EExpr Encoding::EncodeOutflowContains(const FlowGraphNode& node, const std::string& field, const EExpr& value, EMode mode) {
    auto [axiom, dummy] = MakeDummy(node, mode);
    auto predicate = node.parent.config.GetOutflowContains(*axiom, field, dummy);
    return EExpr(Replace(Encode(*predicate), Encode(dummy), value));
}

struct InvariantCreator : public BaseLogicVisitor {
    std::unique_ptr<Invariant> invariant;
    const SolverConfig& config;
    explicit InvariantCreator(const SolverConfig& config) : config(config) {}
    void Visit(const LocalMemoryResource& object) override { invariant = config.GetLocalNodeInvariant(object); }
    void Visit(const SharedMemoryCore& object) override { invariant = config.GetSharedNodeInvariant(object); }
};

EExpr Encoding::EncodeNodeInvariant(const FlowGraphNode& node, EMode mode) {
    InvariantCreator encoder(node.parent.config);
    node.ToLogic(mode)->Accept(encoder);
    if (!encoder.invariant) throw std::logic_error("Internal error: cannot produce invariant for node.");
    return Encode(*encoder.invariant);
}
