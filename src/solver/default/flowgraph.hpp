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
        std::optional<std::reference_wrapper<const heal::SymbolicFlowDeclaration>> preRootOutflow;
        std::optional<std::reference_wrapper<const heal::SymbolicFlowDeclaration>> postAllOutflow;
        std::optional<std::reference_wrapper<const heal::SymbolicFlowDeclaration>> postRootOutflow;

        Field(Field&& other) = default;
        Field(const Field& other) = delete;
    };

    struct FlowGraphNode {
        const heal::SymbolicVariableDeclaration& address;
        bool needed;
        bool preLocal;
        bool postLocal;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> preAllInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> preRootInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> preKeyset;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> postAllInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> postRootInflow;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> postKeyset;
        std::reference_wrapper<const heal::SymbolicFlowDeclaration> frameInflow;
        std::vector<Field> dataFields;
        std::vector<Field> pointerFields;

        FlowGraphNode(FlowGraphNode&& other) = default;
        FlowGraphNode(const FlowGraphNode& other) = delete;

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
    };

    [[nodiscard]] FlowGraph MakeFlowGraph(std::unique_ptr<heal::Annotation> state, const heal::SymbolicVariableDeclaration& rootAddress, const SolverConfig& config, std::size_t depth=1);
    [[nodiscard]] FlowGraph MakeFlowFootprint(std::unique_ptr<heal::Annotation> pre, const cola::Dereference& lhs, const cola::SimpleExpression& rhs, const SolverConfig& config);

    struct EncodedFlowGraph {
        z3::context context;
        z3::solver solver;
        Z3Encoder encoder;
        FlowGraph graph;

        EncodedFlowGraph(const EncodedFlowGraph& other) = delete;
        explicit EncodedFlowGraph(FlowGraph&& graph_);

        z3::expr EncodeKeysetDisjointness(EMode mode);
        z3::expr EncodeInflowUniqueness(EMode mode);
        z3::expr EncodeNodeInvariant(const FlowGraphNode& node, EMode mode);
        z3::expr EncodeNodePredicate(const heal::Predicate& predicate, const FlowGraphNode& node, const z3::expr& argument, EMode mode);
    };

}

#endif
