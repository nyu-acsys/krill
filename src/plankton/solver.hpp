#pragma once
#ifndef PLANKTON_SOLVER
#define PLANKTON_SOLVER


#include <memory>
#include "cola/ast.hpp"
#include "plankton/logic.hpp"
#include "plankton/properties.hpp"


namespace plankton {

	struct FlowDomain {
		virtual ~FlowDomain() = default;

		/** Provides a predicate 'P(node, key)' computing whether or not 'key' is in the outflow of 'node'.
		  * Note: if the outflow contains 'key' only if 'key' is contained in the flow of 'node', then the
		  *       predicate 'P' has to ensure this itself.
		  *
		  * ASSUMPTION: heap are homogeneous, i.e., consist of a single type of nodes only or at least behave
		  *             all equivalently wrt. to the returned outflow predicate.
		  */
		virtual const Predicate& GetOutFlowContains() const = 0;

		// TODO: have another predicate 'P(node, key, succ)' computing whether or not 'node' sends flow containing 'key' to 'succ' (or GetOutFlowContains per selector?)
	};

	struct PostConfig {
		std::unique_ptr<FlowDomain> flowDomain;
		std::unique_ptr<Predicate> logicallyContainsKey;
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

		virtual std::unique_ptr<Annotation> Post(const Formula& pre, const cola::Assume& cmd) const = 0;
		virtual std::unique_ptr<Annotation> Post(const Formula& pre, const cola::Malloc& cmd) const = 0;
		virtual std::unique_ptr<Annotation> Post(const Formula& pre, const cola::Assignment& cmd) const = 0;
		virtual std::unique_ptr<Annotation> PostAssignment(const Formula& pre, const cola::Expression& assignee, const cola::Expression& src) const = 0;

		virtual bool PostEntails(const Formula& pre, const cola::Assume& cmd, const Formula& post) const;
		virtual bool PostEntails(const Formula& pre, const cola::Malloc& cmd, const Formula& post) const;
		virtual bool PostEntails(const Formula& pre, const cola::Assignment& cmd, const Formula& post) const;
	};


	std::unique_ptr<Solver> MakeLinearizabilitySolver(const cola::Program& program);

} // namespace plankton

#endif