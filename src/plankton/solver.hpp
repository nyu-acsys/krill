#pragma once
#ifndef PLANKTON_SOLVER
#define PLANKTON_SOLVER


#include <memory>
#include "cola/ast.hpp"
#include "plankton/logic.hpp"
#include "plankton/properties.hpp"


namespace plankton {

	inline std::vector<std::reference_wrapper<const Tactic>> GetDefaultTactics();


	/** Flow domain base class to parametrize a solvers in (almost) arbitrary flows.
	  *
	  * ASSUMPTION: heaps are homogeneous, i.e., consist of a single type of nodes only.
	  */
	struct FlowDomain {
		virtual ~FlowDomain() = default;

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
		virtual const Predicate& GetOutFlowContains(std::string fieldname) const = 0;

		/** Provides the footprint size, i.e., the number of nodes that are affected by the assignment 'lhs = rhs'
		  * under the configuration characterized by 'pre'.
		  *
		  * NOTE: variable assignments are implicitly assumed to have a footprint size of '0'. To ensure this,
		  *       a solver will most certainly disallow assignments to shared variables.
		  * 
		  * ASSUMPTION: the footprint size accounts for all nodes the flow of which changes and for all nodes
		  *             the fields of which change independent of the fact whether or not their flow changes.
		  */
		virtual std::size_t GetFootprintSize(const Annotation& pre, const cola::Dereference& lhs, const cola::Expression& rhs) const = 0;

		/** TODO: doc
		  */
		virtual std::vector<std::reference_wrapper<const Tactic>> GetTactics() const {
			return GetDefaultTactics();
		}
	};


	/** Configuration object for solvers.
	  */
	struct PostConfig {
		/** The underlying flow domain.
		  */
		std::unique_ptr<FlowDomain> flowDomain;

		/** A predicate 'P(node, key)' computing whether or not 'node' logically contains 'key'.
		  */
		std::unique_ptr<Predicate> logicallyContainsKey;

		/** An invariant 'I(node)' that is implicitly universally quantified over all nodes in the
		  */
		std::unique_ptr<Invariant> invariant;
	};


	/** An iterative implication solver for a given premise P.
	  * Allows to establish implications of the form 'P ==> A'.
	  * Iteratively here refers to the fact that the object may reuse knowledge from previous querys.
	  */
	struct ImplicationChecker {
		virtual ~ImplicationChecker() = default;

		virtual bool ImpliesFalse() const = 0;
		virtual bool Implies(const Formula& implied) const = 0;
		virtual bool Implies(const cola::Expression& implied) const = 0;
		virtual bool ImpliesNonNull(const cola::Expression& nonnull) const = 0;
	};


	/** A solver for checking logical implication and computing post images.
	  * The solver works relative to an invariant that it implicitly assumes and enforces.
	  */
	struct Solver {
		PostConfig config;

		virtual ~Solver() = default;
		Solver(PostConfig config);

		virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker(const Formula& formula) const = 0;

		virtual bool ImpliesFalse(const Formula& formula) const;
		virtual bool Implies(const Formula& formula, const Formula& implied) const;
		virtual bool Implies(const Formula& formula, const cola::Expression& implied) const;
		virtual bool ImpliesNonNull(const Formula& formula, const cola::Expression& nonnull) const;

		virtual std::unique_ptr<Annotation> Join(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) const;
		virtual std::unique_ptr<Annotation> Join(std::vector<std::unique_ptr<Annotation>> annotations) const = 0;

		virtual std::unique_ptr<Annotation> AddInvariant(std::unique_ptr<Annotation> annotation) const = 0;
		virtual std::unique_ptr<Annotation> StripInvariant(std::unique_ptr<Annotation> annotation) const = 0;


		virtual void EnterScope(const cola::Scope& scope) = 0;
		virtual void EnterScope(const cola::Function& function) = 0;
		virtual void EnterScope(const cola::Program& program) = 0;
		virtual void LeaveScope() = 0;
		virtual void LeaveScope(std::size_t amount);

		virtual std::unique_ptr<Annotation> Post(const Annotation& pre, const cola::Assume& cmd) const = 0;
		virtual std::unique_ptr<Annotation> Post(const Annotation& pre, const cola::Malloc& cmd) const = 0;
		virtual std::unique_ptr<Annotation> Post(const Annotation& pre, const cola::Assignment& cmd) const = 0;

		using parallel_assignment_t = std::vector<std::pair<std::reference_wrapper<const cola::Expression>, std::reference_wrapper<const cola::Expression>>>;
		virtual std::unique_ptr<Annotation> Post(const Annotation& pre, parallel_assignment_t assignment) const = 0;

		virtual bool PostEntails(const ConjunctionFormula& pre, const cola::Assignment& cmd, const ConjunctionFormula& post) const;
	};


	std::unique_ptr<Solver> MakeLinearizabilitySolver(const cola::Program& program);

} // namespace plankton

#endif