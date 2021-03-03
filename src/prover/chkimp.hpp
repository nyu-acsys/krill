#pragma once
#ifndef PROVER_CHKIMP
#define PROVER_CHKIMP


#include "cola/ast.hpp"
#include "prover/encoding.hpp"


namespace prover {

	struct ImplicationChecker {
		virtual ~ImplicationChecker() = default;

		virtual void Push() = 0;
		virtual void Pop() = 0;

		virtual void AddPremise(Term term) = 0;
		virtual void AddPremise(const heal::Formula& premise) = 0;

		virtual bool Implies(Term term) const = 0;
		virtual bool Implies(const heal::Formula& implied) const = 0;
		virtual bool Implies(const cola::Expression& implied) const = 0;
		
		virtual bool ImpliesFalse() const = 0;
		virtual bool ImpliesNegated(Term term) const = 0;
		virtual bool ImpliesNegated(const heal::NowFormula& implied) const = 0;
		
		virtual bool ImpliesIsNull(Term term) const = 0;
		virtual bool ImpliesIsNonNull(Term term) const = 0;
		virtual bool ImpliesIsNull(const cola::Expression& expression) const = 0;
		virtual bool ImpliesIsNonNull(const cola::Expression& expression) const = 0;

		virtual std::vector<bool> ComputeImplied(const std::vector<Term>& terms) const = 0;
		virtual std::unique_ptr<heal::ConjunctionFormula> ComputeImpliedConjuncts(const heal::ConjunctionFormula& conjuncts) const = 0;
	};

} // namespace prover

#endif