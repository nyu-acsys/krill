#include "footprint/flowconstraint.hpp"
#include "engine/flowgraph.hpp"
#include "z3++.h"
#include "util/shortcuts.hpp"
#include "engine/encoding.hpp"
#include "util/log.hpp"
#include "engine/util.hpp"
#include <cmath>

using namespace plankton;

//
// Flow Constraint
//

static const constexpr std::initializer_list<EMode> EMODES = { EMode::PRE, EMode::POST };
static const constexpr std::initializer_list<EMode> PRE_EMODE = { EMode::PRE };

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
            auto modes = HasUpdate(edge) ? EMODES : PRE_EMODE;
            for (auto mode : modes) {
                auto successor = graph.GetNode(edge.Value(mode));
                if (footprint.count(successor) > 0) continue;
                result.emplace(&edge, mode);
            }
        }
    }
    return result;
}

// params: encoding, flow constraint, initial footprint (odif / changed nodes), current footprint, outgoing edges
using ExtensionFunction = std::function<EdgeSet(Encoding&, const FlowConstraint&, const NodeSet&, const NodeSet&, const EdgeSet&)>;

std::optional<NodeSet> ComputeFixedPoint(const FlowConstraint& graph, const ExtensionFunction& getFootprintExtension) {
    Encoding encoding(*graph.context);
    auto diff = GetDiff(graph);
    auto footprint = diff;
    while (true) {
        auto edges = GetOutgoingEdges(graph, footprint);
        auto extension = getFootprintExtension(encoding, graph, diff, footprint, edges);
        if (extension.empty()) break;
        for (const auto& [edge, mode] : extension) {
            auto successor = graph.GetNode(edge->Value(mode));
            if (!successor) return std::nullopt;
            footprint.insert(successor);
        }
    }
    return footprint;
}

//
// Acyclicity
//

inline ReachSet ComputeReach(ReachSet initial) {
    ReachSet reachability = std::move(initial);
    bool changed;
    do {
        changed = false;
        for (auto& [node, nodeReach] : reachability.container) {
            assert(node);
            auto size = nodeReach.size();
            for (const auto* value : nodeReach) {
                if (value->type.sort != Sort::PTR) continue;
                const auto& valueReach = reachability.container[value];
                nodeReach.insert(valueReach.begin(), valueReach.end());
            }
            changed |= size != nodeReach.size();
        }
    } while (changed);
    return reachability;
}


ReachSet ComputeReachability(const FlowConstraint& graph, EMode mode) {
    ReachSet initial;
    for (const auto& node : graph.nodes) {
        for (const auto& field : node.pointerSelectors) {
            assert(field.Value(mode).type.sort == Sort::PTR);
            initial.container[&node.address].insert(&field.Value(mode));
        }
    }
    return ComputeReach(std::move(initial));
}

bool MaintainsAcyclicity(Encoding& encoding, const FlowConstraint& graph, const NodeSet& footprint) {
    auto preReach = ComputeReachability(graph, EMode::PRE);
    auto postReach = ComputeReachability(graph, EMode::POST);

    auto isFootprintNode = [&](const auto* address){
        auto node = graph.GetNode(*address);
        if (!node) return false;
        return footprint.find(node) != footprint.end();
    };
    auto isNullPtr = [&](const auto* address) {
        return encoding.Implies(encoding.EncodeIsNull(*address));
    };
    auto unreachableInPreState = [&](const auto* address) -> bool {
        auto node = graph.GetNode(*address);
        return node != nullptr && node->unreachablePre;
    };
    auto isUnreachabilityMaintained = [&](const auto* address) {
        if (!unreachableInPreState(address)) return false;
        return plankton::All(footprint, [&](const auto* node){
            return &node->address == address || node->unreachablePre || !postReach.IsReachable(node->address, *address);
        });
    };

    // no cycle inside footprint
    for (const auto& node : footprint) {
        if (!postReach.IsReachable(node->address, node->address)) continue;
        return false;
    }

    // footprint reaches no new non-footprint nodes
    for (const auto* node : footprint) {
        for (const auto* inReachPost : postReach.GetReachable(node->address)) {
            if (isFootprintNode(inReachPost)) continue;
            if (preReach.IsReachable(node->address, *inReachPost)) continue;
            if (isUnreachabilityMaintained(&node->address)) continue;
            if (isNullPtr(inReachPost)) continue;
            return false;
        }
    }

    // newly reachable footprint-nodes must be previously unreachable
    for (const auto* node : footprint) {
        for (const auto* inReachPost : postReach.GetReachable(node->address)) {
            if (!isFootprintNode(inReachPost)) continue;
            if (unreachableInPreState(inReachPost)) continue;
            if (preReach.IsReachable(node->address, *inReachPost)) continue;
            return false;
        }
    }

    return true;
}

//
// General Method
//

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
                for (const auto& predecessor : footprint) {
                    for (const auto& edge : predecessor->pointerSelectors) {
                        if (edge.Value(mode) != node->address) continue;
                        incoming.push_back(encoding.Encode(edge.Outflow(mode))(qv));
                    }
                }
                return inFlow >> encoding.MakeOr(incoming);
            }));
        }
    }

    return encoding.MakeAnd(result);
}

EExpr MakeInflowBound(Encoding& encoding, const NodeSet& footprint) {
    auto result = plankton::MakeVector<EExpr>(footprint.size());
    for (const auto* node : footprint) {
        if (!node->emptyInflow) continue;
        result.push_back(encoding.Encode(InflowEmptinessAxiom(node->inflow, true)));
    }
    return encoding.MakeAnd(result);
}

EExpr MakeOutflowCheck(Encoding& encoding, const PointerSelector& edge, EMode mode) {
    auto sameOutflow = encoding.Encode(edge.outflowPre) == encoding.Encode(edge.outflowPost);
    auto empty = encoding.Encode(InflowEmptinessAxiom(edge.Outflow(mode), true));
    return HasUpdate(edge) ? empty : sameOutflow;
}

EdgeSet ExtendFootprint_GeneralMethod(Encoding& encoding, const FlowConstraint& graph, const NodeSet& /*init*/, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    auto bound = MakeInflowBound(encoding, footprint);
    auto transfer = MakeGraphTransferFunction(encoding, graph, footprint);

    EdgeSet result;
    for (const auto& [edge, mode] : outgoingEdges) {
        auto outflow = MakeOutflowCheck(encoding, *edge, mode);
        auto check = (bound && transfer) >> outflow;
        if (!encoding.Implies(check)) result.emplace(edge, mode);
    }
    return result;
}

EdgeSet ExtendFootprint_GeneralMethodWithCycleCheck(Encoding& encoding, const FlowConstraint& graph, const NodeSet& init, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    auto result = ExtendFootprint_GeneralMethod(encoding, graph, init, footprint, outgoingEdges);
    if (!result.empty()) return result;
    if (MaintainsAcyclicity(encoding, graph, footprint)) return result;
    return outgoingEdges; // force failure by increasing footprint
}

//
// New Method, full sums
//

PathList MakeAllSimplePaths(const FlowConstraint& graph, const NodeSet& footprint, const NodeSet& initialNodes, EMode mode) {
    std::deque<std::pair<Path, std::set<const Node*>>> worklist;
    for (const auto* node : initialNodes) {
        for (const auto& edge : node->pointerSelectors) {
            worklist.emplace_back();
            worklist.back().first.emplace_back(node, &edge);
        }
    }

    PathList result;
    while (!worklist.empty()) {
        auto [path, visited] = std::move(worklist.front());
        worklist.pop_front();

        auto current = graph.GetNode(path.back().second->Value(mode));
        if (current && visited.find(current) != visited.end()) continue; // not a simple path
        if (!current || footprint.find(current) == footprint.end()) { // next is outside footprint
            result.push_back(path);
        } else { // next is inside footprint (and not yet visited)
            for (const auto& edge : current->pointerSelectors) {
                worklist.emplace_back(path, visited);
                worklist.back().first.emplace_back(current, &edge);
                worklist.back().second.insert(current);
            }
        }
    }

    return result;
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
    if (path.empty() || path.front().first->emptyInflow) return encoding.Bool(false);
    auto result = plankton::MakeVector<EExpr>(path.size());
    result.push_back(encoding.Encode(path.front().first->inflow)(encoding.Encode(symbol)));
    for (const auto& [node, edge] : path)
    {
        result.push_back(EncodeSymbolIsSentOut(encoding, graph, *node, *edge, symbol, mode));
    }
    return encoding.MakeAnd(result);
}

EExpr EncodeSymbolIsSentOut(Encoding& encoding, const FlowConstraint& graph, const PathList& list, const PointerSelector& edge, const SymbolDeclaration& target, const SymbolDeclaration& symbol, EMode mode) {
    auto result = plankton::MakeVector<EExpr>(list.size());
    for (const auto& path : list) {
        if (path.back().second != &edge) continue;
        if (path.back().second->Value(mode) != target) continue;
        result.push_back(EncodeSymbolIsSentOut(encoding, graph, path, symbol, mode));
    }
    return encoding.MakeOr(result);
}

const SymbolDeclaration& MakeDummySymbol(const FlowConstraint& graph) {
    SymbolFactory factory;
    factory.Avoid(*graph.context);
    for (const auto& node : graph.nodes) {
        factory.Avoid(node.address);
        for (const auto& sel : node.dataSelectors) { factory.Avoid(sel.valuePre); factory.Avoid(sel.valuePost); }
        for (const auto& sel : node.pointerSelectors) { factory.Avoid(sel.valuePre); factory.Avoid(sel.valuePost); }
    }
    assert(!graph.nodes.empty());
    return factory.GetFreshFO(graph.nodes.front().inflow.get().type);
}

EdgeSet MakeFootprintExtensionUsingNewMethodBase(Encoding& encoding, const FlowConstraint& graph, const NodeSet& init, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    if (footprint.empty() || outgoingEdges.empty()) return {};
    auto& symbol = MakeDummySymbol(graph);

    EdgeSet result;
    auto prePaths = MakeAllSimplePaths(graph, footprint, init, EMode::PRE);
    auto postPaths = MakeAllSimplePaths(graph, footprint, init, EMode::POST);

    for (const auto& [edge, mode] : outgoingEdges) {
        auto prePathEncoding = EncodeSymbolIsSentOut(encoding, graph, prePaths, *edge, edge->Value(mode), symbol, EMode::PRE);
        auto postPathEncoding = EncodeSymbolIsSentOut(encoding, graph, postPaths, *edge, edge->Value(mode), symbol, EMode::POST);
        auto check = (prePathEncoding == postPathEncoding);
        if (!encoding.Implies(check)) result.emplace(edge, mode);
    }
    return result;
}

EdgeSet ExtendFootprint_NewMethod_AllPathsFullSumWithCycleCheck(Encoding& encoding, const FlowConstraint& graph, const NodeSet& /*init*/, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    auto result = MakeFootprintExtensionUsingNewMethodBase(encoding, graph, footprint, footprint, outgoingEdges);
    if (!result.empty()) return result;
    if (MaintainsAcyclicity(encoding, graph, footprint)) return result;
    return outgoingEdges; // force failure by increasing footprint
}

EdgeSet ExtendFootprint_NewMethod_AllPathsFullSum(Encoding& encoding, const FlowConstraint& graph, const NodeSet& /*init*/, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    return MakeFootprintExtensionUsingNewMethodBase(encoding, graph, footprint, footprint, outgoingEdges);
}

EdgeSet ExtendFootprint_NewMethod_DiffPathsFullSum(Encoding& encoding, const FlowConstraint& graph, const NodeSet& init, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    return MakeFootprintExtensionUsingNewMethodBase(encoding, graph, init, footprint, outgoingEdges);
}

//
// New Method, path replacements
//

EExpr EncodeSymbolIsSentOutForReplacement(Encoding& encoding, const FlowConstraint& graph, const PathList& list, const Node& source,
                                          const PointerSelector& edge, const SymbolDeclaration& target, const SymbolDeclaration& symbol, EMode mode) {
    auto result = plankton::MakeVector<EExpr>(list.size());
    for (const auto& path : list) {
        if (path.front().first != &source) continue;
        if (path.back().second != &edge) continue;
        if (path.back().second->Value(mode) != target) continue;
        result.push_back(EncodeSymbolIsSentOut(encoding, graph, path, symbol, mode));
    }
    return encoding.MakeOr(result);
}

EdgeSet ExtendFootprint_NewMethod_DiffPathsSingleSum(Encoding& encoding, const FlowConstraint& graph, const NodeSet& init, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    if (footprint.empty() || outgoingEdges.empty()) return {};
    auto& symbol = MakeDummySymbol(graph);

    auto prePaths = MakeAllSimplePaths(graph, footprint, init, EMode::PRE);
    auto postPaths = MakeAllSimplePaths(graph, footprint, init, EMode::POST);
    auto getPaths = [&](EMode mode) -> PathList& { return mode == EMode::PRE ? std::ref(prePaths) : std::ref(postPaths); };

    EdgeSet result;
    for (const auto& [edge, mode]: outgoingEdges) {
        auto checks = plankton::MakeVector<EExpr>(16);
        auto otherMode = mode == EMode::PRE ? EMode::POST : EMode::PRE;
        for (const auto& path: getPaths(mode)) {
            if (path.empty()) continue;
            if (path.back().second != edge) continue;
            auto pathEncoded = EncodeSymbolIsSentOut(encoding, graph, path, symbol, mode);
            auto otherPathsEncoded = EncodeSymbolIsSentOutForReplacement(
                    encoding, graph, getPaths(otherMode), *path.front().first,
                    *path.back().second,path.back().second->Value(mode), symbol, otherMode);
            checks.push_back(pathEncoded >> otherPathsEncoded);
        }
        if (!encoding.Implies(encoding.MakeAnd(checks))) result.emplace(edge, mode);
    }
    return result;
}

//
// Eval
//

// void PrintGraph(const FlowConstraint& graph) {
//     for (const auto& node : graph.nodes) {
//         DEBUG("node " << node.address << " : " << node.address.type << std::endl)
//         for (const auto& sel : node.dataSelectors) DEBUG("   - " << sel.name << " = " << sel.valuePre.get() << " / " << sel.valuePost.get() << " : " << sel.type << std::endl)
//         for (const auto& sel : node.pointerSelectors) DEBUG("   + " << sel.name << " = " << sel.valuePre.get() << " / " << sel.valuePost.get() << " : " << sel.type << std::endl)
//     }
//     DEBUG(*graph.context << std::endl)
// }

Evaluation::Evaluation(const FlowConstraint& graph) : constraint(graph), time(0) {}

Evaluation EvaluateFootprintComputationMethod(const FlowConstraint& graph, const ExtensionFunction& method) {
    // PrintGraph(graph);
    Evaluation result(graph);

    auto start = std::chrono::steady_clock::now();
    result.footprint = ComputeFixedPoint(graph, method);
    auto end = std::chrono::steady_clock::now();
    result.time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    return result;
}

Evaluation plankton::Evaluate_GeneralMethod_NoAcyclicityCheck(const FlowConstraint& graph){
    return EvaluateFootprintComputationMethod(graph, ExtendFootprint_GeneralMethod);
}

Evaluation plankton::Evaluate_GeneralMethod_WithAcyclicityCheck(const FlowConstraint& graph){
    return EvaluateFootprintComputationMethod(graph, ExtendFootprint_GeneralMethodWithCycleCheck);
}

Evaluation plankton::Evaluate_NewMethod_DistributivityOnlyWithAcyclicityCheck(const FlowConstraint& graph) {
    return EvaluateFootprintComputationMethod(graph, ExtendFootprint_NewMethod_AllPathsFullSumWithCycleCheck);
}

Evaluation plankton::Evaluate_NewMethod_AllPaths(const FlowConstraint& graph){
    return EvaluateFootprintComputationMethod(graph, ExtendFootprint_NewMethod_AllPathsFullSum);
}

Evaluation plankton::Evaluate_NewMethod_DiffPaths(const FlowConstraint& graph){
    return EvaluateFootprintComputationMethod(graph, ExtendFootprint_NewMethod_DiffPathsFullSum);
}

Evaluation plankton::Evaluate_NewMethod_DiffPathsIndividually(const FlowConstraint& graph){
    return EvaluateFootprintComputationMethod(graph, ExtendFootprint_NewMethod_DiffPathsSingleSum);
}

