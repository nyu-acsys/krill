#pragma once
#ifndef PLANKTON_SOLVERIMPL
#define PLANKTON_SOLVERIMPL


#include <deque>
#include "plankton/solver.hpp"
#include "plankton/solver/encoder.hpp"


namespace plankton {

	class ImplicationCheckerImpl final : public ImplicationChecker {
		private:
			Encoder& encoder;
			mutable z3::solver solver;

			bool Implies(z3::expr expr) const;

		public:
			ImplicationCheckerImpl(Encoder& encoder, const Formula& premise);

			bool ImpliesFalse() const override;
			bool Implies(const Formula& implied) const override;
			bool Implies(const cola::Expression& implied) const override;
			bool ImpliesNonNull(const cola::Expression& nonnull) const override;
	};


	class SolverImpl final : public Solver {
		private:
			mutable Encoder encoder;
			std::deque<std::unique_ptr<ConjunctionFormula>> candidateFormulas;
			std::deque<std::unique_ptr<ConjunctionFormula>> instantiatedInvariants;
			std::deque<std::deque<const cola::VariableDeclaration*>> variablesInScope;

		private:
			inline const ConjunctionFormula& GetCandidates() const { return *candidateFormulas.back(); }
			inline const ConjunctionFormula& GetInstantiatedInvariant() const { return *instantiatedInvariants.back(); }
			inline const std::deque<const cola::VariableDeclaration*>& GetVariablesInScope() const { return variablesInScope.back(); }

			void PushInnerScope();
			void PushOuterScope();
			void ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars);

		public:
			SolverImpl(PostConfig config_) : Solver(std::move(config_)), encoder(config) {
			}

			std::unique_ptr<ImplicationChecker> MakeImplicationChecker(const Formula& formula) const override {
				return std::make_unique<ImplicationCheckerImpl>(encoder, formula);
			}

			std::unique_ptr<Annotation> Join(std::vector<std::unique_ptr<Annotation>> annotations) const override; // TODO: strip invariant, join, then add invariant

			std::unique_ptr<Annotation> AddInvariant(std::unique_ptr<Annotation> annotation) const override;
			std::unique_ptr<Annotation> StripInvariant(std::unique_ptr<Annotation> annotation) const override;


			void EnterScope(const cola::Scope& scope) override;
			void EnterScope(const cola::Function& function) override;
			void EnterScope(const cola::Program& program) override;
			void LeaveScope() override;

			std::unique_ptr<Annotation> Post(const Annotation& pre, const cola::Assume& cmd) const override;
			std::unique_ptr<Annotation> Post(const Annotation& pre, const cola::Malloc& cmd) const override;
			std::unique_ptr<Annotation> Post(const Annotation& pre, const cola::Assignment& cmd) const override;
			std::unique_ptr<Annotation> Post(const Annotation& pre, parallel_assignment_t assignment) const override;

			bool PostEntails(const ConjunctionFormula& pre, const cola::Assignment& cmd, const ConjunctionFormula& post) const override;
	};

} // namespace plankton

#endif