#include "engine/flowgraph.hpp"

#include "util/shortcuts.hpp"

using namespace plankton;


//
// Flow graph basics
//

template<typename T>
inline T& Switch(EMode mode, T& pre, T& post) {
    switch (mode) {
        case EMode::PRE: return pre;
        case EMode::POST: return post;
    }
}

Field::Field(std::string name, const Type& type, const SymbolDeclaration& value)
        : name(std::move(name)), type(type), preValue(value), postValue(value) {
    assert(value.order == Order::FIRST);
}

const SymbolDeclaration& Field::Value(EMode mode) const {
    return Switch(mode, preValue, postValue);
}

PointerField::PointerField(std::string name, const Type& type, const SymbolDeclaration& value,
                           SymbolFactory& factory, const Type& flowType)
        : Field(std::move(name), type, value),
          preAllOutflow(factory.GetFreshSO(flowType)), preGraphOutflow(factory.GetFreshSO(flowType)),
          postAllOutflow(factory.GetFreshSO(flowType)), postGraphOutflow(factory.GetFreshSO(flowType)) {
}

const SymbolDeclaration& PointerField::AllOutflow(EMode mode) const {
    return Switch(mode, preAllOutflow, postAllOutflow);
}

const SymbolDeclaration& PointerField::GraphOutflow(EMode mode) const {
    return Switch(mode, preGraphOutflow, postGraphOutflow);
}

inline const SymbolDeclaration& MkFlow(const FlowGraph& graph, SymbolFactory& factory) {
    return factory.GetFreshSO(graph.config.GetFlowValueType());
}

FlowGraphNode::FlowGraphNode(const FlowGraph& parent, const SymbolDeclaration& address, bool local,
                             const SymbolDeclaration& preFlow, SymbolFactory& factory)
        : parent(parent), address(address), needed(false), preLocal(local), postLocal(local), preAllInflow(preFlow),
          preGraphInflow(MkFlow(parent, factory)), preKeyset(MkFlow(parent, factory)),
          postAllInflow(MkFlow(parent, factory)), postGraphInflow(MkFlow(parent, factory)),
          postKeyset(MkFlow(parent, factory)), frameInflow(MkFlow(parent, factory)) {
    assert(address.order == Order::FIRST);
}

bool FlowGraphNode::IsLocal(EMode mode) const {
    return Switch(mode, preLocal, postLocal);
}

const SymbolDeclaration& FlowGraphNode::AllInflow(EMode mode) const {
    return Switch(mode, preAllInflow, postAllInflow);
}

const SymbolDeclaration& FlowGraphNode::GraphInflow(EMode mode) const {
    return Switch(mode, preGraphInflow, postGraphInflow);
}

const SymbolDeclaration& FlowGraphNode::Keyset(EMode mode) const {
    return Switch(mode, preKeyset, postKeyset);
}

const SymbolDeclaration& FlowGraphNode::FrameInflow() const {
    return frameInflow;
}

const Field& FlowGraphNode::GetField(const std::string& name) const {
    auto search = [&name](auto& container) -> const Field* {
        auto find = plankton::FindIf(container, [&name](auto& elem){ return elem.name == name; });
        if (find != container.end()) return &*find;
        else return nullptr;
    };
    if (auto ptr = search(pointerFields)) return *ptr;
    if (auto other = search(dataFields)) return *other;
    throw std::logic_error("Lookup for field '" + name + "' failed."); // TODO: better error handling
}

Field& FlowGraphNode::GetField(const std::string& name) {
    return const_cast<Field&>(std::as_const(*this).GetField(name));
}

template<typename T>
inline std::unique_ptr<MemoryAxiom> MakeMemoryAxiom(const FlowGraphNode& node, EMode mode) {
    auto address = std::make_unique<SymbolicVariable>(node.address);
    auto flow = std::make_unique<SymbolicVariable>(node.AllInflow(mode));
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& field : node.dataFields)
        fields[field.name] = std::make_unique<SymbolicVariable>(field.Value(mode));
    for (const auto& field : node.pointerFields)
        fields[field.name] = std::make_unique<SymbolicVariable>(field.Value(mode));
    return std::make_unique<T>(std::move(address), std::move(flow), std::move(fields));
}

std::unique_ptr<MemoryAxiom> FlowGraphNode::ToLogic(EMode mode) const {
    if (IsLocal(mode)) return MakeMemoryAxiom<LocalMemoryResource>(*this, mode);
    else return MakeMemoryAxiom<SharedMemoryCore>(*this, mode);
}

FlowGraph::FlowGraph(std::unique_ptr<Annotation> pre_, const SolverConfig& config)
        : config(config), pre(std::move(pre_)) {
    assert(pre);
}

const FlowGraphNode& FlowGraph::GetRoot() const {
    return nodes.at(0);
}

const FlowGraphNode* FlowGraph::GetNodeOrNull(const SymbolDeclaration& address) const {
    auto find = plankton::FindIf(nodes, [&address](auto& elem){ return elem.address == address; });
    if (find != nodes.end()) return &(*find);
    return nullptr;
}

FlowGraphNode* FlowGraph::GetNodeOrNull(const SymbolDeclaration& address) {
    return const_cast<FlowGraphNode*>(std::as_const(*this).GetNodeOrNull(address));
}

std::vector<const PointerField*> FlowGraph::GetIncomingEdges(const FlowGraphNode& target, EMode mode) const {
    std::vector<const PointerField*> result;

    for (const auto& node : nodes) {
        for (const auto& field : node.pointerFields) {
            auto next = GetNodeOrNull(field.Value(mode));
            if (!next || next != &target) continue;
            result.push_back(&field);
        }
    }

    return result;
}
