#pragma once
#ifndef SOLVER_IMPLICATION
#define SOLVER_IMPLICATION

#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "encoding.hpp"

namespace solver {

    struct ImplicationChecker {
        virtual ~ImplicationChecker() = default;

        virtual void Push() = 0;
        virtual void Pop() = 0;

        virtual void AddPremise(Term term) = 0;
        virtual void AddPremise(const heal::Formula& premise) = 0;

        [[nodiscard]] virtual bool Implies(Term term) const = 0;
        [[nodiscard]] virtual bool Implies(const heal::Formula& implied) const = 0;
        [[nodiscard]] virtual bool Implies(const cola::Expression& implied) const = 0;

        [[nodiscard]] virtual bool ImpliesFalse() const = 0;
        [[nodiscard]] virtual bool ImpliesNegated(Term term) const = 0;
        [[nodiscard]] virtual bool ImpliesNegated(const heal::Formula& implied) const = 0;

        [[nodiscard]] virtual bool ImpliesIsNull(Term term) const = 0;
        [[nodiscard]] virtual bool ImpliesIsNonNull(Term term) const = 0;
        [[nodiscard]] virtual bool ImpliesIsNull(const cola::Expression& expression) const = 0;
        [[nodiscard]] virtual bool ImpliesIsNonNull(const cola::Expression& expression) const = 0;

        [[nodiscard]] virtual std::vector<bool> ComputeImplied(const std::vector<Term>& terms) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::SeparatingConjunction> ComputeImpliedConjuncts(const heal::SeparatingConjunction& conjuncts) const = 0;
    };

} // namespace solver

#endif
