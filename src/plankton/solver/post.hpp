#pragma once
#ifndef PLANKTON_SOLVERPOST
#define PLANKTON_SOLVERPOST


#include "cola/ast.hpp"
#include "plankton/logic.hpp"
#include "plankton/solver.hpp"
#include "plankton/solver/solverimpl.hpp"


namespace plankton {

	std::string AssignmentString(const cola::Expression& lhs, const cola::Expression& rhs);
	std::string AssignmentString(const Solver::parallel_assignment_t& assignment);

	void ThrowUnsupportedAssignmentError(const cola::Expression& lhs, const cola::Expression& rhs, std::string moreReason);
	void ThrowUnsupportedAllocationError(const cola::VariableDeclaration& lhs, std::string moreReason);

	void ThrowInvariantViolationError(std::string cmd, std::string more_message="");
	void ThrowInvariantViolationError(const cola::Malloc& cmd);
	void ThrowInvariantViolationError(const cola::Expression& lhs, const cola::Expression& rhs);


	struct PostInfo {
		using time_container_t = decltype(Annotation::time);
		static const time_container_t& GetDummyContainer() { static time_container_t dummy; return dummy; }

		const SolverImpl& solver;
		const ConjunctionFormula& preNow;
		const time_container_t& preTime;
		const ConjunctionFormula& candidates;
		bool implicationCheck; // if true: do not check invariant nor specification, may abort if not all candidates are implied

		PostInfo(const SolverImpl& solver, const Annotation& pre)
			: solver(solver), preNow(*pre.now), preTime(pre.time), candidates(solver.GetCandidates()), implicationCheck(false) {}

		PostInfo(const SolverImpl& solver, const ConjunctionFormula& pre, const ConjunctionFormula& candidates)
			: solver(solver), preNow(pre), preTime(GetDummyContainer()), candidates(candidates), implicationCheck(true) {}

		std::unique_ptr<ConjunctionFormula> ComputeImpliedCandidates(const ImplicationChecker& checker);
	};


	std::unique_ptr<Annotation> PostProcess(std::unique_ptr<Annotation> post, const Annotation& pre);
	std::unique_ptr<Annotation> MakeVarAssignPost(PostInfo info, const Solver::parallel_assignment_t& assignment);
	std::unique_ptr<Annotation> MakeVarAssignPost(PostInfo info, const cola::VariableExpression& lhs, const cola::Expression& rhs);
	std::unique_ptr<Annotation> MakeDerefAssignPost(PostInfo info, const cola::Dereference& lhs, const cola::Expression& rhs);


} // namespace plankton

#endif