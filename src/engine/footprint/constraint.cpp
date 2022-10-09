#include "footprint/flowconstraint.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


BaseSelector::BaseSelector(std::string name, const Type& type, const SymbolDeclaration& valuePre, const SymbolDeclaration& valuePost)
        : name(std::move(name)), type(type), valuePre(valuePre), valuePost(valuePost) {}

const SymbolDeclaration& BaseSelector::Value(EMode mode) const { return mode == EMode::PRE ? valuePre : valuePost; }

PointerSelector::PointerSelector(const std::string& name, const Type& type, const SymbolDeclaration& valuePre,
                                 const SymbolDeclaration& valuePost,
                                 SymbolFactory& factory, const Type& flowType)
        : BaseSelector(name, type, valuePre, valuePost), outflowPre(factory.GetFreshSO(flowType)),
          outflowPost(factory.GetFreshSO(flowType)) {}

const SymbolDeclaration& PointerSelector::Outflow(EMode mode) const { return mode == EMode::PRE ? outflowPre : outflowPost; }

Node::Node(const SymbolDeclaration& address, SymbolFactory& factory, const Type& flowType)
        : address(address), inflow(factory.GetFreshSO(flowType)), flowPre(factory.GetFreshSO(flowType)),
          flowPost(factory.GetFreshSO(flowType)) {}

const SymbolDeclaration& Node::Flow(EMode mode) const { return mode == EMode::PRE ? flowPre : flowPost; }

FlowConstraint::FlowConstraint(const SolverConfig& config, std::unique_ptr<SeparatingConjunction> context) : config(config), context(std::move(context)) {}

const Node *FlowConstraint::GetNode(const SymbolDeclaration& address) const {
    auto find = plankton::FindIf(nodes, [&address](auto& elem) { return elem.address == address; });
    if (find == nodes.end()) return nullptr;
    return &*find;
}

Node *FlowConstraint::GetNode(const SymbolDeclaration& address) {
    return const_cast<Node *>(static_cast<const FlowConstraint&>(*this).GetNode(address));
}