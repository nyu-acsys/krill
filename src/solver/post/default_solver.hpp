#pragma once
#ifndef SOLVER_DEFAULTSOLVER
#define SOLVER_DEFAULTSOLVER

#include "solver/solver.hpp"

namespace solver {

    struct DefaultSolver final : public Solver {
        DefaultSolver(std::shared_ptr<SolverConfig> config, std::unique_ptr<Encoder> encoder);

        [[nodiscard]] std::unique_ptr<ImplicationChecker> MakeImplicationChecker() const override;

        [[nodiscard]] std::unique_ptr<heal::Annotation> Join(std::unique_ptr<heal::Annotation> annotation, std::unique_ptr<heal::Annotation> other) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const override;

        [[nodiscard]] std::unique_ptr<heal::Annotation> Post(const heal::Annotation &pre, const cola::Assume &cmd) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> Post(const heal::Annotation &pre, const cola::Malloc &cmd) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> Post(const heal::Annotation &pre, const cola::Assignment &cmd) const override;

        [[nodiscard]] bool PostEntailsUnchecked(const heal::Formula& pre, const cola::Assignment& cmd, const heal::Formula& post) const override;

        private:
            std::unique_ptr<Encoder> encoder;
    };

}

#endif
