#pragma once
#include <utility>

#include "engine/flowgraph.hpp"
#include "z3++.h"

using namespace plankton;

//
// Flow Constraint
//

// static const constexpr bool EARLY_ABORT = false;
static const constexpr std::initializer_list<EMode> EMODES = { EMode::PRE, EMode::POST };

struct BaseSelector {
    const std::string name;
    const Type& type;
    std::reference_wrapper<const SymbolDeclaration> valuePre, valuePost;
    BaseSelector(std::string name, const Type& type, const SymbolDeclaration& valuePre, const SymbolDeclaration& valuePost)
            : name(std::move(name)), type(type), valuePre(valuePre), valuePost(valuePost) {}
    [[nodiscard]] const SymbolDeclaration& Value(EMode mode) const { return mode == EMode::PRE ? valuePre : valuePost; }
};

struct DataSelector final : public BaseSelector {
    using BaseSelector::BaseSelector;
};

struct PointerSelector final : public BaseSelector {
    std::reference_wrapper<const SymbolDeclaration> outflowPre, outflowPost;
    PointerSelector(const std::string& name, const Type& type, const SymbolDeclaration& valuePre, const SymbolDeclaration& valuePost, SymbolFactory& factory, const Type& flowType)
            : BaseSelector(name, type, valuePre, valuePost), outflowPre(factory.GetFreshSO(flowType)), outflowPost(factory.GetFreshSO(flowType)) {}
    [[nodiscard]] const SymbolDeclaration& Outflow(EMode mode) const { return mode == EMode::PRE ? outflowPre : outflowPost; }
};

struct Node {
    const SymbolDeclaration& address;
    std::reference_wrapper<const SymbolDeclaration> inflow;
    std::reference_wrapper<const SymbolDeclaration> flowPre, flowPost;
    std::deque<DataSelector> dataSelectors;
    std::deque<PointerSelector> pointerSelectors;
    Node(const SymbolDeclaration& address, SymbolFactory& factory, const Type& flowType)
            : address(address), inflow(factory.GetFreshSO(flowType)), flowPre(factory.GetFreshSO(flowType)), flowPost(factory.GetFreshSO(flowType)) {}
    [[nodiscard]] const SymbolDeclaration& Flow(EMode mode) const { return mode == EMode::PRE ? flowPre : flowPost; }
};

struct FlowConstraint {
    const SolverConfig& config;
    SeparatingConjunction& context;
    std::deque<Node> nodes;

    explicit FlowConstraint(const SolverConfig& config, SeparatingConjunction& context) : config(config), context(context) {}

    [[nodiscard]] const Node* GetNode(const SymbolDeclaration& address) const {
        auto find = plankton::FindIf(nodes, [&address](auto& elem){ return elem.address == address; });
        if (find == nodes.end()) return nullptr;
        return &*find;
    }
};

using NodeSet = std::set<const Node*>;
using EdgeSet = std::set<const PointerSelector*>;
using Path = std::deque<std::pair<const Node*, const PointerSelector*>>;
using PathList = std::deque<Path>;

//
// Helpers
//

bool HasUpdate(const BaseSelector& selector) {
    return selector.valuePre.get() != selector.valuePost.get();
}

NodeSet GetDiff(const FlowConstraint& graph) {
    NodeSet result;
    for (const auto& node : graph.nodes) {
        if (!plankton::Any(node.dataSelectors, HasUpdate)
        && !plankton::Any(node.pointerSelectors, HasUpdate))
            continue;
        result.insert(&node);
    }
    return result;
}

EdgeSet GetOutgoingEdges(const FlowConstraint& graph, const NodeSet& footprint) {
    EdgeSet result;
    for (const auto* node : footprint) {
        for (const auto& edge : node->pointerSelectors) {
            auto successorPre = graph.GetNode(edge.valuePre);
            auto successorPost = graph.GetNode(edge.valuePost);
            if (footprint.count(successorPre) > 0 && footprint.count(successorPost) > 0) continue;
            result.insert(&edge);
        }
    }
    return result;
}

using ExtensionFunction = std::function<EdgeSet(Encoding& encoding, const FlowConstraint&, const NodeSet&, const EdgeSet&)>;

std::optional<NodeSet> ComputeFixedPoint(const FlowConstraint& graph, const ExtensionFunction& getFootprintExtension) {
    DEBUG("Starting Fixed Point" << std::endl)
    Encoding encoding(graph.context);
    auto footprint = GetDiff(graph);
    while (true) {
        DEBUG(" - Fixed Point Iteration " << footprint.size() << std::endl)
        auto edges = GetOutgoingEdges(graph, footprint);
        auto extension = getFootprintExtension(encoding, graph, footprint, edges);
        DEBUG("      -> " << extension.size() << "/" << edges.size() << " failed edges" << std::endl)
        if (extension.empty()) break;
        for (const auto* edge : extension) {
            for (auto mode : EMODES) {
                auto successor = graph.GetNode(edge->Value(mode));
                if (!successor) return std::nullopt;
                footprint.insert(successor);
                DEBUG("      -> " << "extending with " << successor->address << std::endl)
            }
        }
    }
    return footprint;
}

//
// General Method
//

struct IncomingMap {
    mutable std::map<const Node*, EdgeSet> pre, post;

    explicit IncomingMap(const FlowConstraint& graph) {
        for (const auto& node : graph.nodes) {
            for (const auto& edge : node.pointerSelectors) {
                auto preSuccessor = graph.GetNode(edge.valuePre);
                auto postSuccessor = graph.GetNode(edge.valuePost);
                if (preSuccessor) pre[preSuccessor].insert(&edge);
                if (postSuccessor) pre[postSuccessor].insert(&edge);
            }
        }
    }

    [[nodiscard]] EdgeSet GetIncoming(const Node& node, EMode mode) const {
        switch (mode) {
            case EMode::PRE: return pre[&node];
            case EMode::POST: return post[&node];
        }
    }
};

EExpr EncodeSubset(Encoding& encoding, const SymbolDeclaration& subset, const SymbolDeclaration& superset) {
    auto sub = encoding.Encode(subset);
    auto super = encoding.Encode(superset);
    return encoding.EncodeForAll(subset.type.sort, [sub,super](auto qv){
        return (sub(qv)) >> (super(qv));
    });
}

EExpr EncodeOutflow(Encoding& encoding, const FlowConstraint& graph, const Node& node, const PointerSelector& edge, EMode mode) {
    std::map<std::string, std::reference_wrapper<const SymbolDeclaration>> fields;
    auto addField = [&fields,mode](const auto& selector) { fields.insert({selector.name, std::cref(selector.Value(mode))}); };
    for (const auto& selector : node.dataSelectors) addField(selector);
    for (const auto& selector : node.pointerSelectors) addField(selector);
    SharedMemoryCore axiom(node.address, node.Flow(mode), fields);
    auto& dummyRaw = SymbolFactory(axiom).GetFreshFO(node.inflow.get().type);
    auto predicateRaw = graph.config.GetOutflowContains(axiom, edge.name, dummyRaw);

    auto dummy = encoding.Encode(dummyRaw);
    auto predicate = encoding.Encode(*predicateRaw);
    return encoding.EncodeForAll(node.inflow.get().type.sort, [&](auto qv){
        auto inFlow = encoding.Encode(node.Flow(mode))(qv);
        auto inEdge = encoding.Replace(predicate, dummy, qv);
        auto inOutflow = encoding.Encode(edge.Outflow(mode))(qv);
        return (inFlow && inEdge) == inOutflow;
    });
}

EExpr MakeGraphTransferFunction(Encoding& encoding, const FlowConstraint& graph, const NodeSet& footprint) {
    auto result = plankton::MakeVector<EExpr>(16);
    result.push_back(encoding.Bool(true));
    IncomingMap incomingMap(graph);

    // outflow is computed from flow
    for (const auto* node : footprint) {
        for (const auto& edge : node->pointerSelectors) {
            result.push_back(EncodeOutflow(encoding, graph, *node, edge, EMode::PRE));
            result.push_back(EncodeOutflow(encoding, graph, *node, edge, EMode::POST));
        }
    }

    // inflow is contained in flow
    for (const auto* node : footprint) {
        // inflow is contained in flow
        result.push_back(EncodeSubset(encoding, node->inflow, node->flowPre));
        result.push_back(EncodeSubset(encoding, node->inflow, node->flowPost));
    }

    // outflow is contained in target's flow
    for (const auto* node : footprint) {
        for (const auto& edge : node->pointerSelectors) {
            for (auto mode : EMODES) {
                auto successor = graph.GetNode(edge.Value(mode));
                if (!successor) continue;
                result.push_back(EncodeSubset(encoding, edge.Outflow(mode), successor->Flow(mode)));
            }
        }
    }

    // flow comes either from inflow or from within footprint
    for (const auto* node : footprint) {
        auto flowSort = node->inflow.get().type.sort;
        for (auto mode : EMODES) {
            result.push_back(encoding.EncodeForAll(flowSort, [&](auto qv) {
                auto inFlow = encoding.Encode(node->Flow(mode))(qv);
                std::vector<EExpr> incoming;
                incoming.push_back(encoding.Encode(node->inflow)(qv));
                for (const auto* edge : incomingMap.GetIncoming(*node, mode)) {
                    incoming.push_back(encoding.Encode(edge->Outflow(mode))(qv));
                }
                return inFlow >> encoding.MakeOr(incoming);
            }));
        }
    }

    return encoding.MakeAnd(result);
}

EExpr MakeOutflowCheck(Encoding& encoding, const PointerSelector& edge) {
    if (HasUpdate(edge)) {
        return encoding.Encode(InflowEmptinessAxiom(edge.outflowPre, true)) &&
               encoding.Encode(InflowEmptinessAxiom(edge.outflowPost, true));
    } else {
        return encoding.Encode(edge.outflowPre) == encoding.Encode(edge.outflowPost);
    }
}

EdgeSet MakeFootprintExtensionUsingGeneralMethod(Encoding& encoding, const FlowConstraint& graph, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    auto transfer = MakeGraphTransferFunction(encoding, graph, footprint);

    EdgeSet result;
    for (const auto* edge : outgoingEdges) {
        auto check = transfer >> MakeOutflowCheck(encoding, *edge);
        if (!encoding.Implies(check)) result.insert(edge);
    }
    return result;
}

//
// New Method, All Paths
//

std::pair<PathList, PathList> MakeAllSimplePaths(const FlowConstraint& graph, const NodeSet& footprint, const NodeSet& initialNodes) {
    std::deque<std::pair<Path, std::set<const Node*>>> worklist;
    for (const auto* node : initialNodes) {
        for (const auto& edge : node->pointerSelectors) {
            worklist.emplace_back();
            worklist.back().first.emplace_back(node, &edge);
        }
    }

    PathList pre, post;
    while (!worklist.empty()) {
        auto [path, visited] = std::move(worklist.front());
        worklist.pop_front();

        for (auto mode : EMODES) {
            auto current = graph.GetNode(path.back().second->Value(mode));
            if (current && visited.find(current) != visited.end()) continue; // not a simple path
            if (!current || footprint.find(current) == footprint.end()) { // next is outside footprint
                auto& dst = mode == EMode::PRE ? pre : post;
                dst.push_back(path);
            } else { // next is inside footprint (and not yet visited)
                for (const auto& edge : current->pointerSelectors) {
                    worklist.emplace_back(path, visited);
                    worklist.back().first.emplace_back(current, &edge);
                    worklist.back().second.insert(current);
                }
            }
        }
    }

    return { std::move(pre), std::move(post) };
}

EExpr EncodeSymbolIsSentOut(Encoding& encoding, const FlowConstraint& graph, const Node& node, const PointerSelector& edge, const SymbolDeclaration& symbol, EMode mode) {
    std::map<std::string, std::reference_wrapper<const SymbolDeclaration>> fields;
    auto addField = [&fields, mode](const auto& selector) { fields.insert({selector.name, std::cref(selector.Value(mode))}); };
    for (const auto& selector: node.dataSelectors) addField(selector);
    for (const auto& selector: node.pointerSelectors) addField(selector);
    SharedMemoryCore axiom(node.address, node.Flow(mode), fields);
    auto predicate = graph.config.GetOutflowContains(axiom, edge.name, symbol);
    return encoding.Encode(*predicate);
}

EExpr EncodeSymbolIsSentOut(Encoding& encoding, const FlowConstraint& graph, const Path& path, const SymbolDeclaration& symbol, EMode mode) {
    auto result = plankton::MakeVector<EExpr>(path.size());
    for (const auto& [node, edge] : path) {
        result.push_back(EncodeSymbolIsSentOut(encoding, graph, *node, *edge, symbol, mode));
    }
    return encoding.MakeAnd(result);
}

EExpr EncodeSymbolIsSentOut(Encoding& encoding, const FlowConstraint& graph, const PathList& list, const PointerSelector& target, const SymbolDeclaration& symbol, EMode mode) {
    auto result = plankton::MakeVector<EExpr>(list.size());
    for (const auto& path : list) {
        if (path.empty() || path.back().second != &target) continue;
        result.push_back(EncodeSymbolIsSentOut(encoding, graph, path, symbol, mode));
    }
    return encoding.MakeOr(result);
}

const SymbolDeclaration& MakeDummySymbol(const FlowConstraint& graph) {
    SymbolFactory factory;
    factory.Avoid(graph.context);
    for (const auto& node : graph.nodes) {
        factory.Avoid(node.address);
        for (const auto& sel : node.dataSelectors) { factory.Avoid(sel.valuePre); factory.Avoid(sel.valuePost); }
        for (const auto& sel : node.pointerSelectors) { factory.Avoid(sel.valuePre); factory.Avoid(sel.valuePost); }
    }
    assert(!graph.nodes.empty());
    return factory.GetFreshFO(graph.nodes.front().inflow.get().type);
}

EdgeSet MakeFootprintExtensionUsingNewMethod(Encoding& encoding, const FlowConstraint& graph, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    if (footprint.empty() || outgoingEdges.empty()) return {};
    auto& symbol = MakeDummySymbol(graph);

    EdgeSet result;
    auto [prePaths, postPaths] = MakeAllSimplePaths(graph, footprint, footprint);
    for (const auto* edge : outgoingEdges) {
        auto prePathEncoding = EncodeSymbolIsSentOut(encoding, graph, prePaths, *edge, symbol, EMode::PRE);
        auto postPathEncoding = EncodeSymbolIsSentOut(encoding, graph, prePaths, *edge, symbol, EMode::POST);
        if (!encoding.Implies(prePathEncoding == postPathEncoding)) result.insert(edge);
    }
    return result;
}

//
// New Method, Changed Paths
//

EdgeSet MakeFootprintExtensionUsingNewOptimizedMethod(Encoding& encoding, const FlowConstraint& graph, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    throw;
}

//
// Eval
//

void EvaluateFootprintComputationMethod(const FlowGraph& graph, const FlowConstraint& constraint, const std::string& name, const ExtensionFunction& method) {
    auto start = std::chrono::steady_clock::now();
    auto footprint = ComputeFixedPoint(constraint, method);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    DEBUG("EvaluateFootprintComputationMethod[" << name << "][" << graph.nodes.size() << "] time=" << elapsed.count() << "ms, size=" << (footprint.has_value() ? std::to_string(footprint->size()) : "-1") << std::endl)
    // TODO: output / return information
}

void EvaluateFootprintComputationMethods(const FlowGraph& graph, const FlowConstraint& constraint) {
    EvaluateFootprintComputationMethod(graph, constraint, "GeneralMethod", MakeFootprintExtensionUsingGeneralMethod);
    EvaluateFootprintComputationMethod(graph, constraint, "NewMethod", MakeFootprintExtensionUsingNewMethod);
    // EvaluateFootprintComputationMethod(graph, constraint, "NewOptimizedMethod", MakeFootprintExtensionUsingNewOptimizedMethod);

    // TODO: output information
}

void ToJson(const FlowGraph& graph, const SolverConfig& config, const std::shared_ptr<EngineSetup>& setup) {
    DEBUG(">>>>>EVAL FOOTPRINT=====" << std::endl)

    // if (!setup || !setup->footprints.is_open()) return;
    // auto& file = setup->footprints;
    // // TODO: write graph to file
    // graph.pre->now.Col

    auto stack = plankton::Collect<StackAxiom>(*graph.pre->now);
    auto context = std::make_unique<SeparatingConjunction>();
    for (auto* elem : stack) context->Conjoin(plankton::Copy(*elem));
    for (const auto& node : graph.nodes) {
        for (const auto& other : graph.nodes) {
            if (&node == &other) continue;
            context->Conjoin(std::make_unique<StackAxiom>(
                    BinaryOperator::NEQ,
                    std::make_unique<SymbolicVariable>(node.address),
                    std::make_unique<SymbolicVariable>(other.address)
            ));
        }
    }

    FlowConstraint constraint(config, *context);
    SymbolFactory factory;
    auto& flowType = config.GetFlowValueType();
    // TODO: create constraint from json
    for (const auto& node : graph.nodes) {
        Node newNode(node.address, factory, flowType);
        for (const auto& field : node.dataFields) {
            DataSelector selector(field.name, field.type, field.preValue, field.postValue);
            newNode.dataSelectors.push_back(std::move(selector));
        }
        for (const auto& field : node.pointerFields) {
            PointerSelector selector(field.name, field.type, field.preValue, field.postValue, factory, flowType);
            newNode.pointerSelectors.push_back(std::move(selector));
        }
        constraint.nodes.push_back(std::move(newNode));
    }

    // hack empty flows
    Encoding encoding(graph);
    for (const auto& node : graph.nodes) {
        InflowEmptinessAxiom empty(node.preAllInflow, true);
        if (encoding.Implies(empty)) {
            DEBUG("empty flow at " << node.address << std::endl)
            if (auto* mapped = constraint.GetNode(node.address)) {
                DEBUG("  -> " << node.address << " maps to " << mapped->address << std::endl)
                constraint.context.Conjoin(std::make_unique<InflowEmptinessAxiom>(mapped->inflow, true));
                constraint.context.Conjoin(std::make_unique<InflowEmptinessAxiom>(mapped->flowPre, true));
            }
        }
    }


    EvaluateFootprintComputationMethods(graph, constraint);

    DEBUG("=====EVAL FOOTPRINT<<<<<" << std::endl)
    // if (graph.nodes.size() > 2)
    throw std::logic_error("---breakpoint---");
}

/*
 * File layout:

 * struct <name> { ... }
 * def @outflow[<sel>](<Struct>* node, data_t key) { ... }
 * graph {
 *  ## decl:type; ...
 *  [address:type] field:type=pre_value,post_value; ...
 *  @@ SMTlib Z3 formula
 * }
 */