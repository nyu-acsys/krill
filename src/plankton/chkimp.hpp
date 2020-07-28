#pragma once
#ifndef PLANKTON_CHKIMP
#define PLANKTON_CHKIMP


#include "cola/ast.hpp"


namespace plankton {

	struct ImplicationChecker {
		virtual ~ImplicationChecker() = default;

		virtual void AddPremise(const heal::Formula& premise) = 0;

		virtual bool Implies(const heal::Formula& implied) const = 0;
		virtual bool Implies(const cola::Expression& implied) const = 0;
		virtual bool ImpliesFalse() const;
		virtual bool ImpliesNegated(const heal::NowFormula& implied) const = 0;
		virtual bool ImpliesIsNull(const cola::Expression& expression) const = 0;
		virtual bool ImpliesIsNonNull(const cola::Expression& expression) const = 0;

		// virtual std::vector<bool> ComputeImplied(const std::vector<std::reference_wrapper<const heal::NowFormula>>& formulas) const = 0;
		virtual std::unique_ptr<heal::ConjunctionFormula> ComputeImpliedConjuncts(const heal::ConjunctionFormula& conjuncts) const = 0;
	};

} // namespace plankton

#endif