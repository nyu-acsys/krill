#pragma once
#ifndef PLANKTON_SOLVERIMPL_POST
#define PLANKTON_SOLVERIMPL_POST


#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "plankton/solverimpl/linsolver.hpp"


namespace plankton {

	std::string AssignmentString(const cola::Expression& lhs, const cola::Expression& rhs);
	std::string AssignmentString(const Solver::parallel_assignment_t& assignment);

	void ThrowUnsupportedAssignmentError(const cola::Expression& lhs, const cola::Expression& rhs, std::string moreReason);
	void ThrowUnsupportedAllocationError(const cola::VariableDeclaration& lhs, std::string moreReason);

	void ThrowInvariantViolationError(std::string cmd, std::string more_message="");
	void ThrowInvariantViolationError(const cola::Malloc& cmd);
	void ThrowInvariantViolationError(const cola::Expression& lhs, const cola::Expression& rhs);


	struct PostInfo {
		using time_container_t = decltype(heal::Annotation::time);
		static const time_container_t& GetDummyContainer() { static time_container_t dummy; return dummy; }

		const SolverImpl& solver;
		const heal::ConjunctionFormula& preNow;
		const time_container_t& preTime;
		const heal::ConjunctionFormula* implication;
		bool implicationCheck;
		bool performInvariantCheck;
		bool performSpecificationCheck;

		PostInfo(const SolverImpl& solver, const heal::Annotation& pre)
			: solver(solver), preNow(*pre.now), preTime(pre.time), implication(nullptr),
			  implicationCheck(false), performInvariantCheck(true), performSpecificationCheck(true) {}

		PostInfo(const SolverImpl& solver, const heal::ConjunctionFormula& pre, const heal::ConjunctionFormula& post)
			: solver(solver), preNow(pre), preTime(GetDummyContainer()), implication(&post),
			  implicationCheck(true), performInvariantCheck(false), performSpecificationCheck(false) {}

		std::unique_ptr<heal::ConjunctionFormula> ComputeImpliedCandidates(const ImplicationChecker& checker);
		time_container_t CopyPreTime() const;
	};


	std::unique_ptr<heal::Annotation> PostProcess(std::unique_ptr<heal::Annotation> post, const heal::Annotation& pre);
	std::unique_ptr<heal::Annotation> MakeVarAssignPost(PostInfo info, const Solver::parallel_assignment_t& assignment);
	std::unique_ptr<heal::Annotation> MakeVarAssignPost(PostInfo info, const cola::VariableExpression& lhs, const cola::Expression& rhs);
	std::unique_ptr<heal::Annotation> MakeDerefAssignPost(PostInfo info, const cola::Dereference& lhs, const cola::Expression& rhs);


} // namespace plankton

#endif