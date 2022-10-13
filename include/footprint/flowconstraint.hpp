#ifndef PLANKTON_FLOWCONSTRAINT_HPP
#define PLANKTON_FLOWCONSTRAINT_HPP

#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/encoding.hpp"
#include "chrono"

namespace plankton {

    struct BaseSelector {
        const std::string name;
        const Type& type;
        std::reference_wrapper<const SymbolDeclaration> valuePre, valuePost;

        BaseSelector(std::string name, const Type& type, const SymbolDeclaration& valuePre, const SymbolDeclaration& valuePost);

        [[nodiscard]] const SymbolDeclaration& Value(EMode mode) const;
    };

    struct DataSelector final : public BaseSelector {
        using BaseSelector::BaseSelector;
    };

    struct PointerSelector final : public BaseSelector {
        std::reference_wrapper<const SymbolDeclaration> outflowPre, outflowPost;

        PointerSelector(const std::string& name, const Type& type, const SymbolDeclaration& valuePre, const SymbolDeclaration& valuePost,
                        SymbolFactory& factory, const Type& flowType);

        [[nodiscard]] const SymbolDeclaration& Outflow(EMode mode) const;
    };

    struct Node {
        bool emptyInflow = false;
        bool unreachablePre = false;
        const SymbolDeclaration& address;
        std::reference_wrapper<const SymbolDeclaration> inflow;
        std::reference_wrapper<const SymbolDeclaration> flowPre, flowPost;
        std::deque <DataSelector> dataSelectors;
        std::deque <PointerSelector> pointerSelectors;

        Node(const SymbolDeclaration& address, SymbolFactory& factory, const Type& flowType);

        [[nodiscard]] const SymbolDeclaration& Flow(EMode mode) const;
    };

    struct FlowConstraint {
        const SolverConfig& config;
        std::unique_ptr<SeparatingConjunction> context;
        std::string name = "--";
        std::deque <Node> nodes;

        explicit FlowConstraint(const SolverConfig& config, std::unique_ptr<SeparatingConjunction> context);

        [[nodiscard]] const Node *GetNode(const SymbolDeclaration& address) const;
        [[nodiscard]] Node *GetNode(const SymbolDeclaration& address);
    };

    using NodeSet = std::set<const Node *>;
    // using EdgeSet = std::set<const PointerSelector *>;
    using EdgeSet = std::set<std::pair<const PointerSelector *, EMode>>;
    using Path = std::deque<std::pair<const Node *, const PointerSelector *>>;
    using PathList = std::deque<Path>;


    struct Evaluation {
        const FlowConstraint& constraint;
        std::optional<NodeSet> footprint;
        std::chrono::nanoseconds time;
        explicit Evaluation(const FlowConstraint& constraint);
    };

    Evaluation Evaluate_GeneralMethod_NoAcyclicityCheck(const FlowConstraint& constraint);
    Evaluation Evaluate_GeneralMethod_WithAcyclicityCheck(const FlowConstraint& constraint);
    Evaluation Evaluate_NewMethod_DistributivityOnlyWithAcyclicityCheck(const FlowConstraint& constraint);
    Evaluation Evaluate_NewMethod_AllPaths(const FlowConstraint& constraint);
    Evaluation Evaluate_NewMethod_DiffPaths(const FlowConstraint& constraint);
    Evaluation Evaluate_NewMethod_DiffPathsIndividually(const FlowConstraint& constraint);

}

#endif //PLANKTON_FLOWCONSTRAINT_HPP
