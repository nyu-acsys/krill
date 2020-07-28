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

		/** May return ture only if a node's ouflow is at most its inflow.
		  *
		  * NOTE: non-decreasing flows may degrade solver performance and/or precision.
		  * // TODO: remove this flag and perform the check within the solver.
		  */
		virtual bool IsFlowDecreasing() const = 0;

		/** The type of nodes in the heap. Thou shalt have no other nodes before this.
		  */
		virtual const cola::Type& GetNodeType() const = 0;

		/** Provides a predicate 'P(node, key)' computing whether or not 'key' is in the outflow of 'node'
		  * via the field named 'fieldname'. This predicate encodes the edge function.
		  * The function will be called for 'fieldname's of pointer sort only.
		  * // TODO: characterize the overall edge function in terms of predicate 'P'
		  *
		  * NOTE: if the outflow contains 'key' only if 'key' is contained in the flow of 'node', then the
		  *       predicate 'P' has to ensure this itself. // TODO: needed?
		  *
		  * ASSUMPTION: edge functions are node-local, i.e., may only access the fields of 'node'.
		  */
		virtual const heal::Predicate& GetOutFlowContains(std::string fieldname) const = 0;

		/** Provides the footprint depth. If the footprint depth is 'D', then its induced footprint are all
		  * nodes that are reachable by taking 'D' edges from 'lhs.expr' in the pre and post heap graph.
		  * That is, the footprint includes nodes reachable from 'lhs.expr' before and after the assignment.
		  * 
		  * The footprint depth is determined dynamically based on the assignment 'lhs = rhs' and the
		  * current heap graph as characterized by 'pre'.
		  *
		  * NOTE: assignments to variables may implicitly be assumed to have a footprint size of '0'.
		  *       To that end, a solver will most certainly disallow assignments to shared variables.
		  */
		virtual std::size_t GetFootprintDepth(const heal::ConjunctionFormula& pre, const cola::Dereference& lhs, const cola::Expression& rhs) const = 0;
	};

} // namespace plankton

#endif