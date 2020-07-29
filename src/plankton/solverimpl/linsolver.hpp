#pragma once
#ifndef PLANKTON_SOLVERIMPL_linsolver
#define PLANKTON_SOLVERIMPL_linsolver


#include <memory>
#include <deque>
#include <vector>
#include <type_traits>
#include "plankton/solver.hpp"
#include "plankton/encoder.hpp"


namespace plankton {

	class SolverImpl final : public Solver {
		private:
			mutable std::shared_ptr<Encoder> encoder;
			std::deque<std::unique_ptr<heal::ConjunctionFormula>> candidates;
			std::deque<std::unique_ptr<heal::ConjunctionFormula>> instantiatedInvariants; // contains rules
			std::deque<std::vector<const cola::VariableDeclaration*>> variablesInScope;

			void PushScope();
			void ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& decls);

		public:
			SolverImpl(PostConfig config);

			inline std::shared_ptr<Encoder> GetEncoder() const { return encoder; }
			inline const heal::ConjunctionFormula& GetCandidates() const { return *candidates.back(); }
			inline const heal::ConjunctionFormula& GetInstantiatedInvariant() const { return *instantiatedInvariants.back(); }
			inline const std::vector<const cola::VariableDeclaration*>& GetVariablesInScope() const { return variablesInScope.back(); }
			
			std::unique_ptr<ImplicationChecker> MakeImplicationChecker() const override;
			std::unique_ptr<ImplicationChecker> MakeImplicationChecker(EncodingTag tag) const;
			std::unique_ptr<ImplicationChecker> MakeImplicationChecker(const heal::Formula& premise) const override;
			std::unique_ptr<heal::ConjunctionFormula> ComputeImpliedCandidates(const heal::Formula& premise) const;
			std::unique_ptr<heal::ConjunctionFormula> ComputeImpliedCandidates(const ImplicationChecker& checker) const;

			std::unique_ptr<heal::Annotation> Join(std::unique_ptr<heal::Annotation> annotation, std::unique_ptr<heal::Annotation> other) const override;
			std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const override;
		
			std::unique_ptr<heal::Annotation> AddInvariant(std::unique_ptr<heal::Annotation> annotation) const override;
			std::unique_ptr<heal::Annotation> StripInvariant(std::unique_ptr<heal::Annotation> annotation) const override;
			std::unique_ptr<heal::ConjunctionFormula> AddInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const override;
			std::unique_ptr<heal::ConjunctionFormula> StripInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const override;

			void EnterScope(const cola::Scope& scope) override;
			void EnterScope(const cola::Macro& macro) override;
			void EnterScope(const cola::Function& function) override;
			void EnterScope(const cola::Program& program) override;
			void LeaveScope() override;

			std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Assume& cmd) const override;
			std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Malloc& cmd) const override;
			std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Assignment& cmd) const override;
			std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, parallel_assignment_t assignment) const override;
			bool UncheckedPostEntails(const heal::ConjunctionFormula& pre, const cola::Assignment& cmd, const heal::ConjunctionFormula& post) const override;
	};


	std::unique_ptr<SolverImpl> MakeSolverImpl(const cola::Program& program);

} // namespace plankton

#endif