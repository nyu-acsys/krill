#pragma once
#ifndef PLANKTON_SOLVER
#define PLANKTON_SOLVER


#include <memory>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/properties.hpp"
#include "plankton/flow.hpp"
#include "plankton/chkimp.hpp"


namespace plankton {

	/** Configuration for solvers.
	  */
	struct PostConfig {
		/** Suggests an upper bound for the footprint size beyond which a solver may give up.
		  */
		std::size_t maxFootprintSize;

		/** The underlying flow domain.
		  */
		std::unique_ptr<FlowDomain> flowDomain;

		/** A predicate 'P(node, key)' computing whether or not 'node' logically contains 'key'.
		  */
		std::unique_ptr<heal::Predicate> logicallyContainsKey;

		/** An invariant 'I(node)' that is implicitly universally quantified over all nodes in the
		  */
		std::unique_ptr<heal::Invariant> invariant;
	};


	/** A solver for checking logical implication and computing post images.
	  * The solver works relative to an invariant that it implicitly assumes and enforces.
	  */
	struct Solver {
		const PostConfig config;

		virtual ~Solver() = default;
		Solver(PostConfig config);

		virtual bool ImpliesFalse(const heal::Formula& formula) const;
		virtual bool ImpliesFalseQuick(const heal::Formula& formula) const;
		virtual bool Implies(const heal::Formula& formula, const heal::Formula& implied) const;
		virtual bool Implies(const heal::Formula& formula, const cola::Expression& implied) const;
		virtual bool ImpliesIsNull(const heal::Formula& formula, const cola::Expression& nonnull) const;
		virtual bool ImpliesIsNonNull(const heal::Formula& formula, const cola::Expression& nonnull) const;

		virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker() const = 0;
		virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker(const heal::Formula& premise) const = 0;

		virtual std::unique_ptr<heal::Annotation> Join(std::unique_ptr<heal::Annotation> annotation, std::unique_ptr<heal::Annotation> other) const;
		virtual std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const = 0;

		virtual std::unique_ptr<heal::Annotation> AddInvariant(std::unique_ptr<heal::Annotation> annotation) const = 0;
		virtual std::unique_ptr<heal::Annotation> StripInvariant(std::unique_ptr<heal::Annotation> annotation) const = 0;
		virtual std::unique_ptr<heal::ConjunctionFormula> AddInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const = 0;
		virtual std::unique_ptr<heal::ConjunctionFormula> StripInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const = 0;

		virtual void EnterScope(const cola::Scope& scope) = 0;
		virtual void EnterScope(const cola::Macro& macro) = 0;
		virtual void EnterScope(const cola::Function& function) = 0;
		virtual void EnterScope(const cola::Program& program) = 0;
		virtual void LeaveScope() = 0;

		virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Assume& cmd) const = 0;
		virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Malloc& cmd) const = 0;
		virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Assignment& cmd) const = 0;

		using parallel_assignment_t = std::vector<std::pair<std::reference_wrapper<const cola::VariableExpression>, std::reference_wrapper<const cola::Expression>>>;
		virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, parallel_assignment_t assignment) const = 0;

		/** Implementation dependent: may not check invariant nor specification
		  */
		virtual bool UncheckedPostEntails(const heal::ConjunctionFormula& pre, const cola::Assignment& cmd, const heal::ConjunctionFormula& post) const;
	};


	std::unique_ptr<Solver> MakeLinearizabilitySolver(const cola::Program& program);

} // namespace plankton

#endif