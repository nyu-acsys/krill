#pragma once
#ifndef SOLVER_CONFIG
#define SOLVER_CONFIG


#include <memory>
#include "cola/ast.hpp"
#include "heal/logic.hpp"


namespace solver {

    struct SolverConfig {
        explicit SolverConfig() = default;
        virtual ~SolverConfig() = default;

        /**
         * Retrieves the type of members of second-order flow variables.
         * @return The type of flow values.
         */
        [[nodiscard]] virtual const cola::Type& GetFlowValueType() const = 0;

        /**
         * Suggests a maximum depth beyond which a solver may not explore a (potential) flow footprint.
         * @param updatedFieldName The field name the update of which leads to a flow footprint being constructed.
         * @return A maximal depth suggestion.
         */
        [[nodiscard]] virtual std::size_t GetMaxFootprintDepth(const std::string& updatedFieldName) const = 0;

        /**
         * An invariant 'I(node)' that is implicitly universally quantified over all nodes in the heap.
         * @param memory The node that the invariant should be instantiated for.
         * @return Instantiated invariant.
         */
        [[nodiscard]] virtual std::unique_ptr<heal::Formula> GetNodeInvariant(const heal::PointsToAxiom& memory) const = 0;

        /**
         * An invariant 'I(variable)' that is implicitly universally quantified over all shared variables.
         * @param variable The variable that the invariant should be instantiated for.
         * @return Instantiated invariant.
         */
        [[nodiscard]] virtual std::unique_ptr<heal::Formula> GetSharedVariableInvariant(const heal::EqualsToAxiom& variable) const = 0;

        /**
         * A predicate 'P(node, val)' computing whether or not 'val' is in the outflow of 'node'
		 * via the field named 'fieldName'. This predicate encodes the edge function.
         *
         * The type of 'value' is expected to be the one provided by 'GetFlowValueType()'.
         * The type of 'fieldName' is expected to be of pointer sort.
		 *
		 * NOTE: the outflow is constrained by the inflow of node, i.e., the outflow is precisely
		 *       the set '{ v | v ∈ Inflow(node) && P(node, v) }'.
		 *
         * @param memory The node the outflow of which shall be computed.
         * @param fieldName The field of memory the outflow of which shall be computed.
         * @param value The value that is tested for being in the outflow of fieldName.
         * @return Instantiated predicate.
         */
        [[nodiscard]] virtual std::unique_ptr<heal::Formula> GetOutflowContains(const heal::PointsToAxiom& memory, const std::string& fieldName,
                                                                                const heal::SymbolicVariableDeclaration& value) const = 0;

        /**
         * A predicate 'P(node, key)' computing whether or not 'node' logically contains 'key'.
         * @param memory The node for which containment shall be computed.
         * @param value The value that is tested for being contained.
         * @return Instantiated predicate.
         */
        [[nodiscard]] virtual std::unique_ptr<heal::Formula> GetLogicallyContains(const heal::PointsToAxiom& memory,
                                                                                  const heal::SymbolicVariableDeclaration& value) const = 0;
    };

} // namespace solver

#endif