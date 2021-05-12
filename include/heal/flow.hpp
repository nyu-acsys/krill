#pragma once
#ifndef HEAL_FLOW
#define HEAL_FLOW


#include <memory>
#include "cola/ast.hpp"
#include "logic.hpp"
#include "properties.hpp"


namespace heal {

	/** Flow domain base class to parametrize a solvers in decreasing flows.
	  *
	  * ASSUMPTION: heaps are homogeneous, i.e., consist of a single type of nodes only.
	  */
	struct FlowDomain {
		virtual ~FlowDomain() = default;

		/** The type of nodes in the heap graph. Thou shalt have no other nodes before this.
		  */
		[[nodiscard]] virtual const cola::Type& GetNodeType() const = 0;

		/** The type of flow values.
		  */
		[[nodiscard]] virtual const cola::Type& GetFlowValueType() const = 0;

		/** Provides a predicate 'P(node, val)' computing whether or not 'val' is in the outflow of 'node'
		  * via the field named 'fieldName'. This predicate encodes the edge function. The types of 'node'
		  * and 'val' are those provided by 'GetNodeType()' and 'GetFlowValueType()', respectively.
		  * The function will be called for 'fieldName's of pointer sort only.
		  *
		  * NOTE: the outflow is constrained by the inflow of node, i.e., the outflow is precisely
		  *       the set '{ v | v ∈ Inflow(node) && P(node, v) }'.
		  *
		  * ASSUMPTION: edge functions are node-local, i.e., may only access the fields of 'node'.
		  */
		[[nodiscard]] virtual const Predicate& GetOutFlowContains(std::string fieldName) const = 0;
	};

} // namespace heal

#endif