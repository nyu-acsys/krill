#pragma once
#ifndef PLANKTON_SOLVERIMPL
#define PLANKTON_SOLVERIMPL


#include <set>
#include <stack>
#include <deque>
#include "plankton/solver.hpp"
#include "plankton/solver/encoder.hpp"


namespace plankton {

	class ImplicationCheckerImpl final : public ImplicationChecker {
		private:
			Encoder& encoder;
			mutable z3::solver solver;

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
			std::stack<std::unique_ptr<ConjunctionFormula>> instantiatedInvariants;
			std::stack<std::unique_ptr<ConjunctionFormula>> candidateFormulas;
			std::stack<std::set<const cola::VariableDeclaration*>> variablesInScope;

			void PushScope();
			void ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars);

		public:
			SolverImpl(PostConfig config_) : Solver(std::move(config_)), encoder(config) {}

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

			std::unique_ptr<Annotation> Post(const Formula& pre, const cola::Assume& cmd) const override;
			std::unique_ptr<Annotation> Post(const Formula& pre, const cola::Malloc& cmd) const override;
			std::unique_ptr<Annotation> Post(const Formula& pre, const cola::Assignment& cmd) const override;
			std::unique_ptr<Annotation> PostAssignment(const Formula& pre, const cola::Expression& assignee, const cola::Expression& src) const override;

			bool PostEntails(const Formula& pre, const cola::Assume& cmd, const Formula& post) const override;
			bool PostEntails(const Formula& pre, const cola::Malloc& cmd, const Formula& post) const override;
			bool PostEntails(const Formula& pre, const cola::Assignment& cmd, const Formula& post) const override;
	};

} // namespace plankton

#endif