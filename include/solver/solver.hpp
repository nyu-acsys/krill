#pragma once
#ifndef SOLVER_SOLVER
#define SOLVER_SOLVER


#include <memory>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/properties.hpp"
#include "heal/flow.hpp"
#include "config.hpp"
#include "chkimp.hpp"
#include "encoding.hpp"
#include "encoder.hpp"


namespace solver {

    // TODO: what is an effect? what does it say/mean?
    struct Effect {
        std::unique_ptr<heal::Annotation> change;
        std::unique_ptr<heal::Annotation> context;
        const cola::Assignment& command;

        explicit Effect(std::unique_ptr<heal::Formula> change, std::unique_ptr<heal::Formula> context, const cola::Assignment& command);
        explicit Effect(std::unique_ptr<heal::Annotation> change, std::unique_ptr<heal::Annotation> context, const cola::Assignment& command);
    };

	/** A solver for post images. The solver works relative to an invariant that it implicitly assumes and enforces.
	  */
	struct Solver {
		explicit Solver(std::shared_ptr<SolverConfig> config);
        virtual ~Solver() = default;

//		[[nodiscard]] virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker() const = 0;
//      [[nodiscard]] virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker(const heal::Formula& premise) const;

        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Join(std::unique_ptr<heal::Annotation> annotation, std::unique_ptr<heal::Annotation> other) const;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const = 0;

        /** Post image establishing invariant and specification */
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Assume& cmd) const;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Post(std::unique_ptr<heal::Annotation> pre, const cola::Assume& cmd) const = 0;

        /** Post image establishing invariant and specification */
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Malloc& cmd) const;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Post(std::unique_ptr<heal::Annotation> pre, const cola::Malloc& cmd) const = 0;

        /** Post image establishing invariant and specification */
        [[nodiscard]] virtual std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> Post(const heal::Annotation& pre, const cola::Assignment& cmd) const;
        [[nodiscard]] virtual std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> Post(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd) const = 0;

		/** Post image entailment check; invariant and specification may not be established/checked */
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> MakeStable(const heal::Annotation& pre, const Effect& interference) const;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> MakeStable(std::unique_ptr<heal::Annotation> pre, const Effect& interference) const = 0;

	    protected:
            [[nodiscard]] const SolverConfig& Config() const { return *config; }

        private:
	        std::shared_ptr<SolverConfig> config;
	};

    [[nodiscard]] std::unique_ptr<Solver> MakeDefaultSolver(std::shared_ptr<SolverConfig> config);
//    [[nodiscard]] std::unique_ptr<Solver> MakeDefaultSolver(std::shared_ptr<SolverConfig> config, std::unique_ptr<Encoder> encoder);

} // namespace solver

#endif