#pragma once
#ifndef PLANKTON_SOLVERIMPL
#define PLANKTON_SOLVERIMPL


#include <memory>
#include <deque>
#include <vector>
#include <type_traits>
#include "plankton/solver.hpp"
#include "plankton/solver/encoder.hpp"


namespace plankton {

	class ImplicationCheckerImpl final : public ImplicationChecker {
		private:			
			Encoder& encoder;
			mutable z3::solver solver;
			const Encoder::StepTag encodingTag;
			const NowFormula* premiseNowFormula;
			const Annotation* premiseAnnotation;

		public:
			ImplicationCheckerImpl(Encoder& encoder, const NowFormula& premise, Encoder::StepTag tag = Encoder::StepTag::NOW);
			ImplicationCheckerImpl(Encoder& encoder, const Annotation& premise, Encoder::StepTag tag = Encoder::StepTag::NOW);
			ImplicationCheckerImpl(Encoder& encoder, z3::solver solver, const NowFormula& premise, Encoder::StepTag tag);
			ImplicationCheckerImpl(Encoder& encoder, z3::solver solver, const Annotation& premise, Encoder::StepTag tag);

			// underapproximates implication, may perform a purely syntactical check
			bool ImpliesQuick(const NowFormula& implied) const;
			bool ImpliesNegated(const NowFormula& implied) const;

			bool Implies(const NowFormula& implied) const;
			bool Implies(const PastPredicate& implied) const;
			bool Implies(const FuturePredicate& implied) const;
			bool Implies(const TimePredicate& implied) const;
			bool Implies(const Annotation& implied) const;

			bool ImpliesFalse() const override;
			bool Implies(const Formula& implied) const override;
			bool Implies(const cola::Expression& implied) const override;
			bool ImpliesNonNull(const cola::Expression& nonnull) const override;

			bool Implies(z3::expr expr) const;
			static bool Implies(z3::solver& solver, z3::expr expr);
	};


	class Candidate {
			std::unique_ptr<ConjunctionFormula> store;
			std::unique_ptr<ConjunctionFormula> disprove;
			Candidate(std::unique_ptr<ConjunctionFormula> store, std::unique_ptr<ConjunctionFormula> disprove);

		public:
			Candidate(std::unique_ptr<SimpleFormula> candidate);
			Candidate(std::unique_ptr<SimpleFormula> candidate, std::unique_ptr<ConjunctionFormula> implied);
			Candidate(std::unique_ptr<SimpleFormula> candidate, std::unique_ptr<ConjunctionFormula> implied, std::deque<std::unique_ptr<SimpleFormula>> disprove);
			Candidate Copy() const;

			 // if F implies 'GetCheck()', then F implies 'GetImplied()'
			const SimpleFormula& GetCheck() const;
			const ConjunctionFormula& GetImplied() const;
			
			// if F implies 'GetQuickDisprove()[i]' for some 'i', then F does not imply 'GetCheck()'
			const std::deque<std::unique_ptr<SimpleFormula>>& GetQuickDisprove() const;
	};


	class SolverImpl final : public Solver {
		private:
			mutable Encoder encoder;
			std::deque<std::vector<Candidate>> candidates;
			std::deque<std::unique_ptr<ConjunctionFormula>> instantiatedInvariants;
			std::deque<std::vector<const cola::VariableDeclaration*>> variablesInScope;

		public: // export useful functionality
			inline Encoder& GetEncoder() const { return encoder; }
			inline const std::vector<Candidate>& GetCandidates() const { return candidates.back(); }
			inline const ConjunctionFormula& GetInstantiatedInvariant() const { return *instantiatedInvariants.back(); }
			inline const std::vector<const cola::VariableDeclaration*>& GetVariablesInScope() const { return variablesInScope.back(); }

			void PushInnerScope();
			void PushOuterScope();
			void ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars);

		public: // implement 'Solver' interface
			SolverImpl(PostConfig config_);

			std::unique_ptr<ImplicationCheckerImpl> MakeImplicationChecker(const NowFormula& formula) const;
			std::unique_ptr<ImplicationCheckerImpl> MakeImplicationChecker(const Annotation& annotation) const;
			std::unique_ptr<ImplicationChecker> MakeImplicationChecker(const Formula& formula) const override;

			std::unique_ptr<ConjunctionFormula> ComputeImpliedCandidates(const ImplicationCheckerImpl& checker) const;
			std::unique_ptr<ConjunctionFormula> ComputeImpliedCandidates(const std::vector<ImplicationCheckerImpl>& checkers) const;
			std::unique_ptr<Annotation> Join(std::vector<std::unique_ptr<Annotation>> annotations) const override;

			std::unique_ptr<Annotation> AddInvariant(std::unique_ptr<Annotation> annotation) const override;
			std::unique_ptr<Annotation> StripInvariant(std::unique_ptr<Annotation> annotation) const override;
			std::unique_ptr<ConjunctionFormula> AddInvariant(std::unique_ptr<ConjunctionFormula> formula) const;
			std::unique_ptr<ConjunctionFormula> StripInvariant(std::unique_ptr<ConjunctionFormula> formula) const;

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