#pragma once
#ifndef SOLVER_ENCODER_Z3CHECKIMP
#define SOLVER_ENCODER_Z3CHECKIMP


#include "z3++.h"
#include "solver/chkimp.hpp"
#include "z3encoder.hpp"


namespace solver {

	class Z3ImplicationChecker final : public ImplicationChecker {
		public:
			Z3ImplicationChecker(std::shared_ptr<Z3Encoder> encoder, EncodingTag tag, z3::solver solver);

			void Push() override;
			void Pop() override;

			void AddPremise(Z3Expr expr);
			void AddPremise(Term term) override;
			void AddPremise(const heal::Formula& premise) override;

			[[nodiscard]] bool Implies(Z3Expr expr) const;
			[[nodiscard]] bool Implies(Term term) const override;
			[[nodiscard]] bool Implies(const heal::Formula& implied) const override;
			[[nodiscard]] bool Implies(const cola::Expression& implied) const override;
			
			[[nodiscard]] bool ImpliesFalse() const override;
			[[nodiscard]] bool ImpliesNegated(Term term) const override;
			[[nodiscard]] bool ImpliesNegated(const heal::Formula& implied) const override;
			
			[[nodiscard]] bool ImpliesIsNull(Term term) const override;
			[[nodiscard]] bool ImpliesIsNonNull(Term term) const override;
			[[nodiscard]] bool ImpliesIsNull(const cola::Expression& expression) const override;
			[[nodiscard]] bool ImpliesIsNonNull(const cola::Expression& expression) const override;

			[[nodiscard]] std::vector<bool> ComputeImplied(const std::vector<Z3Expr>& exprs) const;
			[[nodiscard]] std::vector<bool> ComputeImplied(const std::vector<Term>& terms) const override;
			[[nodiscard]] std::unique_ptr<heal::ConjunctionFormula> ComputeImpliedConjuncts(const heal::ConjunctionFormula& conjuncts) const override;
			
		private:
			std::shared_ptr<Z3Encoder> encoderPtr;
			Z3Encoder& encoder;
			EncodingTag encodingTag;
			mutable z3::solver solver;
	};

} // namespace solver

#endif