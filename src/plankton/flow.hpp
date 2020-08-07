#pragma once
#ifndef PLANKTON_FLOW
#define PLANKTON_FLOW


#include <memory>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/properties.hpp"


namespace plankton {

	/** Flow domain base class to parametrize a solvers in (almost) arbitrary flows.
	  *
	  * ASSUMPTION: heaps are homogeneous, i.e., consist of a single type of nodes only.
	  */
	struct FlowDomain {
		virtual ~FlowDomain() = default;

		/** The type of nodes in the heap graph. Thou shalt have no other nodes before this.
		  */
		virtual const cola::Type& GetNodeType() const = 0;

		/** The type of flow values.
		  */
		virtual const cola::Type& GetFlowValueType() const = 0;

		/** Provides a predicate 'P(node, val)' computing whether or not 'val' is in the outflow of 'node'
		  * via the field named 'fieldname'. This predicate encodes the edge function. The types of 'node'
		  * and 'val' are those provided by 'GetNodeType()' and 'GetFlowValueType()', respectively.
		  * The function will be called for 'fieldname's of pointer sort only.
		  *
		  * NOTE: if the outflow contains 'val' only if 'val' is contained in the flow of 'node', then the
		  *       predicate 'P' has to ensure this itself.
		  *
		  * ASSUMPTION: edge functions are node-local, i.e., may only access the fields of 'node'.
		  */
		virtual const heal::Predicate& GetOutFlowContains(std::string fieldname) const = 0;

		/** May return ture only if a node's ouflow never exceeds its flow.
		  * If the function is not overriden an approximate check is performed; it is advisable to prove
		  * or disprove the flow decreasing in the meta theory and simply return the appropriate flag.
		  *
		  * NOTE: non-decreasing flows may degrade solver performance and/or precision.
		  */
		virtual bool IsFlowDecreasing() const;
	};

} // namespace plankton

#endif