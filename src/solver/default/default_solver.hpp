#pragma once
#ifndef SOLVER_DEFAULTSOLVER
#define SOLVER_DEFAULTSOLVER

#include "solver/solver.hpp"

namespace solver {

    struct DefaultSolver final : public Solver {
        using Solver::Solver;

        [[nodiscard]] std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const override;

        [[nodiscard]] PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Assume& cmd) const override;
        [[nodiscard]] PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Malloc& cmd) const override;
        [[nodiscard]] PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd) const override;

        [[nodiscard]] PostImage PostVariableUpdate(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd, const cola::VariableExpression& lhs) const;
        [[nodiscard]] PostImage PostMemoryUpdate(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd, const cola::Dereference& lhs) const;

        [[nodiscard]] std::unique_ptr<heal::Annotation> MakeStable(std::unique_ptr<heal::Annotation> pre, const Effect& interference) const override;
    };

}

#endif
