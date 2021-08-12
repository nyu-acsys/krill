#include "engine/flowgraph.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "util/log.hpp"

using namespace plankton;


constexpr std::array<EMode, 2> AllEMode = { EMode::PRE, EMode::POST };

struct UpdateMap {
    using update_t = std::pair<const Dereference*, const VariableExpression*>;
    using update_list_t = std::vector<update_t>;
    using update_map_t = std::map<const VariableDeclaration*, update_list_t>;
    update_map_t lookup;
    
    explicit UpdateMap(const MemoryWrite& command) {
        assert(command.lhs.size() == command.rhs.size());
        for (std::size_t index = 0; index < command.lhs.size(); ++index) {
            auto dereference = command.lhs.at(index).get();
            auto variable = &dereference->variable->Decl();
            auto value = command.rhs.at(index).get();
            lookup[variable].emplace_back(dereference, value);
        }
    }
    
    [[nodiscard]] const update_list_t& GetUpdates(const VariableDeclaration& decl) const {
        static update_list_t empty = {};
        auto find = lookup.find(&decl);
        if (find != lookup.end()) return find->second;
        else return empty;
    }
};

struct Worklist {
    std::set<std::pair<std::size_t, FlowGraphNode*>, std::greater<>> container;
    
    explicit Worklist() = default;
    explicit Worklist(std::size_t size, FlowGraphNode& node) { Add(size, node); }
    
    [[nodiscard]] inline bool Empty() const { return container.empty(); }
    
    inline bool Add(std::size_t size, FlowGraphNode& node) {
        return container.emplace(size, &node).second;
    }
    
    inline std::pair<std::size_t, FlowGraphNode*> Take() {
        auto element = *container.begin();
        container.erase(container.begin());
        return element;
    }
};

inline std::deque<std::set<const SymbolDeclaration*>> ComputeAllRootPaths(const FlowGraph& graph) {
    std::deque<std::set<const SymbolDeclaration*>> result;
    std::deque<std::pair<const FlowGraphNode*, std::set<const SymbolDeclaration*>>> worklist;
    worklist.push_back({&graph.GetRoot(), {}});
    
    while (!worklist.empty()) {
        auto [node, set] = worklist.front();
        set.insert(&node->address);
        bool hasNext = false;
        for (const auto& pointerField : node->pointerFields) {
            for (auto mode : AllEMode) {
                if (auto next = graph.GetNodeOrNull(pointerField.Value(mode))) {
                    hasNext = true;
                    worklist.emplace_back(next, set);
                }
            }
        }
        if (!hasNext) result.push_back(std::move(set));
        worklist.pop_front();
    }
    
    return result;
}

inline std::unique_ptr<StackAxiom> MakeNeq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
    return std::make_unique<StackAxiom>(BinaryOperator::NEQ, std::make_unique<SymbolicVariable>(decl),
                                        std::make_unique<SymbolicVariable>(other));
}

struct FlowGraphGenerator {
    const UpdateMap& updates;
    FlowGraph& graph;
    SeparatingConjunction& state;
    SymbolFactory factory;
    Encoding encoding;
    
    explicit FlowGraphGenerator(FlowGraph& empty, const UpdateMap& updates)
            : updates(updates), graph(empty), state(*empty.pre->now) {
    }
    
    inline bool IsDistinct(const SymbolDeclaration& address) {
        std::vector<EExpr> vector;
        vector.reserve(graph.nodes.size());
        auto expr = encoding.Encode(address);
        for (const auto& node : graph.nodes) vector.push_back(encoding.Encode(node.address) != expr);
        return encoding.Implies(encoding.MakeAnd(vector));
    }
    
    inline FlowGraphNode MakeNodeFromResource(const MemoryAxiom& axiom) {
        auto& flowType = axiom.flow->Type();
        FlowGraphNode result(axiom.node->Decl(), plankton::IsLocal(axiom), axiom.flow->Decl(), factory, flowType);
    
        result.dataFields.reserve(axiom.fieldToValue.size());
        result.pointerFields.reserve(axiom.fieldToValue.size());
        for (const auto&[name, value] : axiom.fieldToValue) {
            if (value->Sort() == Sort::PTR) {
                result.pointerFields.emplace_back(name, value->Type(), value->Decl(), factory, flowType);
            } else {
                result.dataFields.emplace_back(name, value->Type(), value->Decl());
            }
        }
    
        return result;
    }
    
    inline void ApplyUpdates(FlowGraphNode& node) {
        std::set<std::string> updatedFields;
        auto checkUpdate = [&updatedFields](const std::string& field) {
            auto insertion = updatedFields.insert(field);
            if (insertion.second) return;
            throw std::logic_error("Unsupported assignment: updates field '" + field + "' twice."); // TODO: better error handling
        };
        
        // TODO: syntactic check sufficient?
        auto isSameAsNode = [&node](auto& var){ return var.Value() == node.address; };
        auto variables = plankton::Collect<EqualsToAxiom>(state, isSameAsNode);
        for (const auto& variable : variables) {
            for (const auto& [lhs, rhs] : updates.GetUpdates(variable->Variable())) {
                assert(node.address == plankton::Evaluate(*lhs->variable, state));
                checkUpdate(lhs->fieldName);
                auto& field = node.GetField(lhs->fieldName);
                auto& newValue = plankton::Evaluate(*rhs, state);
                field.postValue = newValue;
            }
        }
    }
    
    FlowGraphNode* TryGetOrCreateNode(const SymbolDeclaration& nextAddress) {
        // find the memory resource referenced by nextAddress, abort if none exists
        auto* resource = plankton::TryGetResource(nextAddress, state);
        if (!resource) return nullptr;

        // search for an existing node at the given address, care for aliasing
        auto& alias = resource->node->Decl();
        if (auto find = graph.GetNodeOrNull(alias)) return find;
        if (!IsDistinct(alias)) return nullptr;

        // create a new node, add to footprint
        auto newNode = MakeNodeFromResource(*resource);
        ApplyUpdates(newNode);
        graph.nodes.push_back(std::move(newNode));
        return &graph.nodes.back();
    }

    inline void InlineAliases(FlowGraphNode& node) {
        auto getPointerValue = [&](const SymbolDeclaration& address) -> const SymbolDeclaration& {
            if (auto node = graph.GetNodeOrNull(address)) return node->address;
            if (auto memory = plankton::TryGetResource(address, state)) return memory->node->Decl();
            return address;
        };
        for (auto& field : node.pointerFields) {
            field.preValue = getPointerValue(field.preValue);
            field.postValue = getPointerValue(field.postValue);
        }
    };
    
    std::set<const SymbolDeclaration*> ExpandGraph(std::size_t maxDepth) {
        if (maxDepth == 0) return { };
        Worklist worklist(maxDepth, graph.nodes.front());
        std::set<const SymbolDeclaration*> missingFrontier;

        // expand until maxDepth is reached
        while (!worklist.Empty()) {
            auto [depth, node] = worklist.Take();
            InlineAliases(*node);
            if (depth == 0) continue;

            bool publish = !node->postLocal;
            for (auto& field : node->pointerFields) {
                for (auto mode : AllEMode) {
                    auto& nextAddress = field.Value(mode);
                    if (auto nextNode = TryGetOrCreateNode(nextAddress)) {
                        if (publish) nextNode->postLocal = false;
                        worklist.Add(depth - 1, *nextNode);
                    } else {
                        missingFrontier.insert(&nextAddress);
                    }
                }
            }
        }
        return missingFrontier;
    }
    
    void CheckGraph() const {
        // cyclic flow graphs are not supported // TODO: support cycles
        auto& root = graph.GetRoot();
        if (!graph.GetIncomingEdges(root, EMode::PRE).empty() ||
            !graph.GetIncomingEdges(root, EMode::POST).empty()) {
            throw std::logic_error("Unsupported update: cyclic flow graphs are not supported");
        }
    }
    
    inline FlowGraphNode& MakeRoot(const VariableDeclaration& root) {
        auto& rootAddress = plankton::GetResource(root, state).Value();
        FlowGraphNode* rootNode = TryGetOrCreateNode(rootAddress);
        if (!rootNode) throw std::logic_error("Cannot initialize flow graph."); // TODO: better error handling
        rootNode->needed = true;
        rootNode->preGraphInflow = rootNode->preAllInflow;
        rootNode->postGraphInflow = rootNode->preAllInflow;
        rootNode->postAllInflow = rootNode->preAllInflow;
        return *rootNode;
    }
    
    inline void DeriveFrontierKnowledge(const std::set<const SymbolDeclaration*>& frontier) {
        // get new memory
        auto& flowType = graph.config.GetFlowValueType();
        plankton::MakeMemoryAccessible(state, frontier, flowType, factory, encoding);
        encoding.AddPremise(encoding.EncodeInvariants(state, graph.config)); // for newly added memory
        
        // no cycles => nodes on a path are all distinct
        auto paths = ComputeAllRootPaths(graph);
        for (const auto& path : paths) {
            for (auto it = path.begin(); it != path.end(); ++it) {
                for (auto ot = std::next(it); ot != path.end(); ++it) {
                    auto distinct = MakeNeq(**it, **ot);
                    encoding.AddPremise(*distinct);
                    state.Conjoin(std::move(distinct));
                }
            }
        }
        
        // nodes with same address => same fields
        // prune duplicate memory
        plankton::ExtendStack(*graph.pre, encoding, ExtensionPolicy::POINTERS);
        assert(&state == graph.pre->now.get());
        plankton::InlineAndSimplify(state);
        throw std::logic_error("not yet implemented"); // TODO: inline same/duplicate memory
    }
    
    void Construct(const VariableDeclaration& root, std::size_t depth) {
        
        std::set<const SymbolDeclaration*> frontier;
        while (true) {
            encoding.Push();
            encoding.AddPremise(state); // TODO: encoding.AddPremise(graph) for more precision (trades in runtime)?
            encoding.AddPremise(encoding.EncodeInvariants(state, graph.config));
    
            MakeRoot(root);
            auto newFrontier = ExpandGraph(depth);
            if (frontier == newFrontier) break;
            frontier = std::move(newFrontier);
            
            DeriveFrontierKnowledge(frontier);
            encoding.Pop();
            graph.nodes.clear();
        }
        CheckGraph();
    }
};

FlowGraph plankton::MakeFlowFootprint(std::unique_ptr<Annotation> pre, const MemoryWrite& command, const SolverConfig& config) {
    assert(!command.lhs.empty());
    auto& root = command.lhs.front()->variable->Decl();
    std::size_t depth = 0;
    for (const auto& lhs : command.lhs)
        depth = std::max(depth, config.GetMaxFootprintDepth(lhs->fieldName));
    
    UpdateMap updateMap(command);
    FlowGraph graph(std::move(pre), config);
    FlowGraphGenerator generator(graph, updateMap);
    generator.Construct(root, depth);
    
    DEBUG("Footprint: " << std::endl)
    for (const auto& node : graph.nodes) {
        DEBUG("   - Node " << node.address.name << std::endl)
        for (const auto& next : node.pointerFields) {
            DEBUG("      - " << node.address.name << "->" << next.name << " == " << next.preValue.get().name << " / "
                             << next.postValue.get().name << std::endl)
        }
    }

    return graph;
}

FlowGraph plankton::MakePureHeapGraph(std::unique_ptr<Annotation> state, const SolverConfig& config) {
    throw;

//    heal::InlineAndSimplify(*state);
//    //    std::cout << "Making pure heap graph for: " << *state << std::endl;
//    FlowGraph graph{config, {}, std::move(state)};
//
//    SymbolicFactory factory(*graph.pre);
//    LazyValuationMap eval(*graph.pre);
//
//    // add all nodes
//    auto memory = heal::CollectMemory(*graph.pre);
//    for (const auto* mem : memory) {
//        TryGetOrCreateNode(graph, eval, factory, mem->node->Decl());
//    }
//
//    // make pre/post state equal
//    for (auto& node : graph.nodes) {
//        node.needed = true;
//        node.postAllInflow = node.preAllInflow;
//        node.postKeyset = node.preKeyset;
//        node.postGraphInflow = node.preGraphInflow;
//
//        for (auto& field : node.pointerFields) {
//            field.postAllOutflow = field.preAllOutflow;
//            field.postGraphOutflow = field.preGraphOutflow;
//        }
//    }
//
//    // begin debug
//    //    std::cout << "Pure heap graph:" << std::endl;
//    //    for (const auto& node : graph.nodes) {
//    //        std::cout << "   - Node " << node.address.name << std::endl;
//    //        for (const auto& next : node.pointerFields) {
//    //            std::cout << "      - " << node.address.name << "->" << next.name << " == " << next.preValue.get().name << " / " << next.postValue.get().name << std::endl;
//    //        }
//    //    }
//    //    std::cout << "graph.pre: " << *graph.pre << std::endl;
//    // end debug
//
//    // TODO: find a root (some node that is not reachable from any other node)?
//    return graph;
}
