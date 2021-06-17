#ifndef SOLVER_HPP
#define SOLVER_HPP

#include "z3++.h"
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"
#include "solver/solver.hpp"
#include "encoder.hpp"

namespace solver {

    enum struct EMode { PRE, POST };

    struct Field {
        const std::string name;
        const cola::Type& type;
        std::reference_wrapper<const heal::SymbolicVariableDeclaration> preValue;
        std::reference_wrapper<const heal::SymbolicVariableDeclaration> postValue;
        std::optional<std::reference_wrapper<const heal::SymbolicFlowDeclaration>> preAllOutflow;
        std::optional<std::reference_wrapper<const heal::SymbolicFlowDeclaration>> preGraphOutflow;
        std::optional<std::reference_wrapper<const heal::SymbolicFlowDeclaration>> postAllOutflow;
        std::optional<std::reference_wrapper<const heal::SymbolicFlowDeclaration>> postGraphOutflow;

        Field(Field&& other) = default;
        Field(const Field& other) = delete;
        Field(std::string name, const cola::Type& type, const heal::SymbolicVariableDeclaration& value)
                : name(std::move(name)), type(type), preValue(value), postValue(value), preAllOutflow(std::nullopt),
                  preGraphOutflow(std::nullopt), postAllOutflow(std::nullopt), postGraphOutflow(std::nullopt) {}
    };

    struct FlowGraphNode {
        const heal::SymbolicVariableDeclaration& address;
        bool needed;
        bool preLocal;
        bool postLocal;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> preAllInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> preGraphInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> preKeyset;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> postAllInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> postGraphInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> postKeyset;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> frameInflow;
        std::vector<Field> dataFields;
        std::vector<Field> pointerFields;

        FlowGraphNode(FlowGraphNode&& other) = default;
        FlowGraphNode(const FlowGraphNode& other) = delete;
        FlowGraphNode(const heal::SymbolicVariableDeclaration& address, bool local, const heal::SymbolicFlowDeclaration& preFlow,
                      heal::SymbolicFactory& factory, const cola::Type& flowType)
                : address(address), needed(false), preLocal(local), postLocal(local), preAllInflow(preFlow),
                  preGraphInflow(factory.GetUnusedFlowVariable(flowType)), preKeyset(factory.GetUnusedFlowVariable(flowType)),
                  postAllInflow(factory.GetUnusedFlowVariable(flowType)), postGraphInflow(factory.GetUnusedFlowVariable(flowType)),
                  postKeyset(factory.GetUnusedFlowVariable(flowType)), frameInflow(factory.GetUnusedFlowVariable(flowType)) {}

        [[nodiscard]] std::unique_ptr<heal::PointsToAxiom> ToLogic(EMode mode) const;
    };

    struct FlowGraph {
        const solver::SolverConfig& config;
        std::deque<FlowGraphNode> nodes;
        std::unique_ptr<heal::Annotation> pre;

        FlowGraph(FlowGraph&& other) = default;
        FlowGraph(const FlowGraph& other) = delete;
        [[nodiscard]] FlowGraphNode* GetNodeOrNull(const heal::SymbolicVariableDeclaration& address);
        [[nodiscard]] const FlowGraphNode* GetNodeOrNull(const heal::SymbolicVariableDeclaration& address) const;
        [[nodiscard]] const FlowGraphNode& GetRoot() const { return nodes.at(0); }
        [[nodiscard]] std::vector<const Field*> GetIncomingEdges(const FlowGraphNode& node, EMode mode) const;
    };

    // TODO: add invariants to flow graph
    [[nodiscard]] FlowGraph MakePureHeapGraph(std::unique_ptr<heal::Annotation> state, const SolverConfig& config);
    [[nodiscard]] FlowGraph MakeFlowFootprint(std::unique_ptr<heal::Annotation> pre, const cola::Dereference& lhs, const cola::SimpleExpression& rhs, const SolverConfig& config);

    struct EncodedFlowGraph {
        z3::context context;
        z3::solver solver;
        Z3Encoder<> encoder;
        FlowGraph graph;

        EncodedFlowGraph(const EncodedFlowGraph& other) = delete;
        explicit EncodedFlowGraph(FlowGraph&& graph_);
        z3::expr EncodeKeysetDisjointness(EMode mode);
        z3::expr EncodeInflowUniqueness(const FlowGraphNode& node, EMode mode);
        z3::expr EncodeNodeInvariant(const FlowGraphNode& node, EMode mode);
        z3::expr EncodeNodeOutflow(const FlowGraphNode& node, const std::string& fieldName, const z3::expr& value, EMode mode);
        z3::expr EncodeNodeContains(const FlowGraphNode& node, const z3::expr& value, EMode mode);
    };

}

#endif
