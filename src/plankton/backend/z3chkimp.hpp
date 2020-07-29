#pragma once
#ifndef PLANKTON_BACKEND_Z3CHECKIMP
#define PLANKTON_BACKEND_Z3CHECKIMP


#include "z3++.h"
#include "plankton/chkimp.hpp"
#include "plankton/backend/z3encoder.hpp"


namespace plankton {

	class Z3ImplicationChecker final : public ImplicationChecker {
		public:
			Z3ImplicationChecker(std::shared_ptr<Z3Encoder> encoder, EncodingTag tag, z3::solver solver);

			void Push() override;
			void Pop() override;

			void AddPremise(Z3Expr expr);
			void AddPremise(Term term) override;
			void AddPremise(const heal::Formula& premise) override;

			bool Implies(Z3Expr expr) const;
			bool Implies(Term term) const override;
			bool Implies(const heal::Formula& implied) const override;
			bool Implies(const cola::Expression& implied) const override;
			
			bool ImpliesFalse() const override;
			bool ImpliesNegated(Term term) const override;
			bool ImpliesNegated(const heal::NowFormula& implied) const override;
			
			bool ImpliesIsNull(Term term) const override;
			bool ImpliesIsNonNull(Term term) const override;
			bool ImpliesIsNull(const cola::Expression& expression) const override;
			bool ImpliesIsNonNull(const cola::Expression& expression) const override;

			std::vector<bool> ComputeImplied(const std::vector<Z3Expr>& exprs) const;
			std::vector<bool> ComputeImplied(const std::vector<Term>& terms) const override;
			std::unique_ptr<heal::ConjunctionFormula> ComputeImpliedConjuncts(const heal::ConjunctionFormula& conjuncts) const override;
			
		private:
			std::shared_ptr<Z3Encoder> encoderPtr;
			Z3Encoder& encoder;
			EncodingTag encodingTag;
			mutable z3::solver solver;
	};

} // namespace plankton

#endif