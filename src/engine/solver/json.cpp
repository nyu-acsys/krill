#pragma once
#include <utility>

#include "engine/flowgraph.hpp"
#include "z3++.h"
#include "../encoding/internal.hpp"

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
    bool emptyInflow = false;
    bool unreachablePre = false;
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
    [[nodiscard]] Node* GetNode(const SymbolDeclaration& address) {
        return const_cast<Node*>(static_cast<const FlowConstraint&>(*this).GetNode(address));
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

// params: encoding, flow constraint, initial footprint (odif / changed nodes), current footprint, outgoing edges
using ExtensionFunction = std::function<EdgeSet(Encoding&, const FlowConstraint&, const NodeSet&, const NodeSet&, const EdgeSet&)>;

std::optional<NodeSet> ComputeFixedPoint(const FlowConstraint& graph, const ExtensionFunction& getFootprintExtension) {
    // DEBUG("Starting Fixed Point" << std::endl)
    Encoding encoding(graph.context);
    auto diff = GetDiff(graph);
    auto footprint = diff;
    while (true) {
        // DEBUG(" - Fixed Point Iteration " << footprint.size() << std::endl)
        auto edges = GetOutgoingEdges(graph, footprint);
        auto extension = getFootprintExtension(encoding, graph, diff, footprint, edges);
        // DEBUG("      -> " << extension.size() << "/" << edges.size() << " failed edges" << std::endl)
        if (extension.empty()) break;
        for (const auto* edge : extension) {
            for (auto mode : EMODES) {
                auto successor = graph.GetNode(edge->Value(mode));
                if (!successor) return std::nullopt;
                footprint.insert(successor);
                // DEBUG("      -> " << "extending with " << successor->address << std::endl)
            }
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

EExpr MakeOutflowCheck(Encoding& encoding, const PointerSelector& edge) {
    if (HasUpdate(edge)) {
        return encoding.Encode(InflowEmptinessAxiom(edge.outflowPre, true)) &&
               encoding.Encode(InflowEmptinessAxiom(edge.outflowPost, true));
    } else {
        return encoding.Encode(edge.outflowPre) == encoding.Encode(edge.outflowPost);
    }
}

EdgeSet ExtendFootprint_GeneralMethod(Encoding& encoding, const FlowConstraint& graph, const NodeSet& /*init*/, const NodeSet& footprint, const EdgeSet& outgoingEdges) {
    auto bound = MakeInflowBound(encoding, footprint);
    auto transfer = MakeGraphTransferFunction(encoding, graph, footprint);

    EdgeSet result;
    for (const auto* edge : outgoingEdges) {
        auto outflow = MakeOutflowCheck(encoding, *edge);
        auto check = (bound && transfer) >> outflow;
        if (!encoding.Implies(check)) result.insert(edge);
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
    // result.push_back(encoding.Encode(path.front().first->inflow)(encoding.Encode(symbol)));
    for (const auto& [node, edge] : path) {
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
    factory.Avoid(graph.context);
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

    for (const auto* edge : outgoingEdges) {
        auto prePathPreTargetEncoding = EncodeSymbolIsSentOut(encoding, graph, prePaths, *edge, edge->valuePre, symbol, EMode::PRE);
        auto postPathPreTargetEncoding = EncodeSymbolIsSentOut(encoding, graph, postPaths, *edge, edge->valuePre, symbol, EMode::POST);
        auto prePathPostTargetEncoding = EncodeSymbolIsSentOut(encoding, graph, prePaths, *edge, edge->valuePost, symbol, EMode::PRE);
        auto postPathPostTargetEncoding = EncodeSymbolIsSentOut(encoding, graph, postPaths, *edge, edge->valuePost, symbol, EMode::POST);
        auto check = (prePathPreTargetEncoding == postPathPreTargetEncoding) && (prePathPostTargetEncoding == postPathPostTargetEncoding);
        if (!encoding.Implies(check)) result.insert(edge);
    }
    return result;
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
    for (const auto* edge : outgoingEdges) {
        auto checks = plankton::MakeVector<EExpr>(16);
        for (auto mode : EMODES) {
            auto otherMode = mode == EMode::PRE ? EMode::POST : EMode::PRE;
            for (const auto& path : getPaths(mode)) {
                if (path.empty()) continue;
                if (path.back().second != edge) continue;
                auto pathEncoded = EncodeSymbolIsSentOut(encoding, graph, path, symbol, mode);
                auto otherPathsEncoded = EncodeSymbolIsSentOutForReplacement(encoding, graph, getPaths(otherMode), *path.front().first, *path.back().second,
                                                                             path.back().second->Value(mode), symbol, otherMode);
                checks.push_back(pathEncoded >> otherPathsEncoded);
            }
        }
        if (!encoding.Implies(encoding.MakeAnd(checks))) result.insert(edge);
    }
    return result;
}

//
// Eval
//

int EvaluateFootprintComputationMethod(const FlowGraph& graph, const FlowConstraint& constraint, const std::string& name, const ExtensionFunction& method) {
    auto start = std::chrono::steady_clock::now();
    auto footprint = ComputeFixedPoint(constraint, method);
    for (std::size_t index = 0; index < 99; ++index) ComputeFixedPoint(constraint, method);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start)/100;

    DEBUG("EvaluateFootprintComputationMethod[" << name << "][" << graph.nodes.size() << "] time=" << elapsed.count() << "μs, size=" << (footprint.has_value() ? std::to_string(footprint->size()) : "-1") << std::endl)
    // TODO: output / return information
    return footprint.has_value() ? footprint->size() : -1;
}

void EvaluateFootprintComputationMethods(const FlowGraph& graph, const FlowConstraint& constraint) {
    auto s1 = EvaluateFootprintComputationMethod(graph, constraint, "GeneralMethod                 ", ExtendFootprint_GeneralMethod);
    auto s2 = EvaluateFootprintComputationMethod(graph, constraint, "GeneralMethod--WithCycleCheck ", ExtendFootprint_GeneralMethodWithCycleCheck);
    auto s3 = EvaluateFootprintComputationMethod(graph, constraint, "NewMethod--AllPaths+FullSum   ", ExtendFootprint_NewMethod_AllPathsFullSum);
    auto s4 = EvaluateFootprintComputationMethod(graph, constraint, "NewMethod--DiffPaths+FullSum  ", ExtendFootprint_NewMethod_DiffPathsFullSum);
    auto s5 = EvaluateFootprintComputationMethod(graph, constraint, "NewMethod--DiffPaths+SingleSum", ExtendFootprint_NewMethod_DiffPathsSingleSum);

    // if (s1 != s2) throw std::logic_error("Methods disagree.");
    // if (s2 != s3) throw std::logic_error("Methods disagree.");
    // if (s3 != s4) throw std::logic_error("Methods disagree.");
    // if (s4 != s5) throw std::logic_error("Methods disagree.");
    // TODO: output information
}

//
// Output
//

// struct DataSelector final : public BaseSelector {
//     const std::string name;
//     const Type& type;
// };
//
// struct PointerSelector final : public BaseSelector {
//     const std::string name;
//     const Type& type;
//     std::reference_wrapper<const SymbolDeclaration> outflowPre, outflowPost;
// };
//
// struct Node {
//     bool emptyInflow = false;
//     bool unreachablePre = false;
//     const SymbolDeclaration& address;
//     std::reference_wrapper<const SymbolDeclaration> inflow;
//     std::reference_wrapper<const SymbolDeclaration> flowPre, flowPost;
//     std::deque<DataSelector> dataSelectors;
//     std::deque<PointerSelector> pointerSelectors;
// };
//
// struct FlowConstraint {
//     const SolverConfig& config;
//     SeparatingConjunction& context;
//     std::deque<Node> nodes;
// };

std::deque<std::shared_ptr<Axiom>> MakeContext(const FlowGraph& graph) {
    std::deque<std::shared_ptr<Axiom>> result;

    // get relevant symbols
    std::set<const SymbolDeclaration*> symbols;
    for (const auto& node : graph.nodes) {
        plankton::InsertInto(plankton::CollectUsefulSymbols(*node.ToLogic(EMode::PRE)), symbols);
        plankton::InsertInto(plankton::CollectUsefulSymbols(*node.ToLogic(EMode::POST)), symbols);
    }

    // get implied stack axioms
    auto candidates = plankton::MakeStackCandidates(*graph.pre->now, ExtensionPolicy::FAST);
    Encoding encoding(graph);
    for (auto& elem : candidates) {
        if (!dynamic_cast<const StackAxiom*>(elem.get())) continue;
        if (plankton::EmptyIntersection(symbols, plankton::Collect<SymbolDeclaration>(*elem))) continue;
        encoding.AddCheck(encoding.Encode(*elem), [&](bool holds){
            if (holds) result.push_back(std::move(elem));
        });
    }
    encoding.Check();

    // add separation
    for (const auto& node : graph.nodes) {
        for (const auto& other : graph.nodes) {
            if (&node == &other) continue;
            result.push_back(std::make_unique<StackAxiom>(
                    BinaryOperator::NEQ,
                    std::make_unique<SymbolicVariable>(node.address),
                    std::make_unique<SymbolicVariable>(other.address)
            ));
        }
    }

    return result;
}

//
// Write File
//

void StoreOutput(const FlowGraph& graph, const std::shared_ptr<EngineSetup>& setup) {
    auto context = MakeContext(graph);

    Encoding encoding(graph);
    auto hasNoFlow = [&encoding](const auto& node) {
        InflowEmptinessAxiom empty(node.preAllInflow, true);
        return encoding.Implies(empty);
    };

    auto& out = setup->footprints;
    auto printField = [&out](const auto& field) {
        out << "        " << field.name << " : " << field.type << " = " << field.preValue.get() << " / " << field.postValue.get() << ";" << std::endl;
    };

    out << std::endl << std::endl;
    out << "graph { " << std::endl;
    for (const auto& node : graph.nodes) {
        out << "    node[" << node.address << " : " << node.address.type << "] {" << std::endl;
        if (node.preLocal) out << "        @pre-unreachable;" << std::endl;
        if (hasNoFlow(node)) out << "        @pre-emptyInflow;" << std::endl;
        for (const auto& field : node.dataFields) printField(field);
        for (const auto& field : node.pointerFields) printField(field);
        out << "    }" << std::endl;
    }
    for (const auto& axiom : context) {
        out << "    @constraint: " << *axiom << ";" << std::endl;
    }
    out << "}" << std::endl;
}


//
// Temp
//

void ToJson(const FlowGraph& graph, const SolverConfig& config, const std::shared_ptr<EngineSetup>& setup) {
    DEBUG(">>>>>EVAL FOOTPRINT=====" << std::endl)

    // if (!setup || !setup->footprints.is_open()) return;
    // auto& file = setup->footprints;
    // // TODO: write graph to file
    // graph.pre->now.Col

    std::set<const SymbolDeclaration*> symbols;
    for (const auto& node : graph.nodes) {
        plankton::InsertInto(plankton::CollectUsefulSymbols(*node.ToLogic(EMode::PRE)), symbols);
        plankton::InsertInto(plankton::CollectUsefulSymbols(*node.ToLogic(EMode::POST)), symbols);
    }

    auto candidates = plankton::MakeStackCandidates(*graph.pre->now, ExtensionPolicy::FAST);
    auto context = std::make_unique<SeparatingConjunction>();
    Encoding enc(graph);
    for (auto& elem : candidates) {
        if (!dynamic_cast<const StackAxiom*>(elem.get())) continue;
        if (plankton::EmptyIntersection(symbols, plankton::Collect<SymbolDeclaration>(*elem))) continue;
        enc.AddCheck(enc.Encode(*elem), [&](bool holds){ if (holds) context->Conjoin(std::move(elem)); });
    }
    enc.Check();
    // auto stack = plankton::Collect<StackAxiom>(*graph.pre->now);
    // auto context = std::make_unique<SeparatingConjunction>();
    // for (auto* elem : stack) context->Conjoin(plankton::Copy(*elem));
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
        if (node.preLocal) newNode.unreachablePre = true;
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
                mapped->emptyInflow = true;
                // constraint.context.Conjoin(std::make_unique<InflowEmptinessAxiom>(mapped->inflow, true));
                // constraint.context.Conjoin(std::make_unique<InflowEmptinessAxiom>(mapped->flowPre, true));
            }
        }
    }


    EvaluateFootprintComputationMethods(graph, constraint);

    DEBUG("=====EVAL FOOTPRINT<<<<<" << std::endl)
    // if (graph.nodes.size() > 2) throw std::logic_error("---breakpoint---");
    // if (graph.nodes.size() > 1) throw std::logic_error("---breakpoint---");
    // throw std::logic_error("---breakpoint---");
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