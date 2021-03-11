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

	/** A solver for post images. The solver works relative to an invariant that it implicitly assumes and enforces.
	  */
	struct Solver {
		explicit Solver(std::shared_ptr<SolverConfig> config) : config(std::move(config)) {}
        virtual ~Solver() = default;

        [[nodiscard]] const SolverConfig& Config() const { return *config; }

		[[nodiscard]] virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker() const = 0;
        [[nodiscard]] virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker(const heal::Formula& premise) const;

        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Join(std::unique_ptr<heal::Annotation> annotation, std::unique_ptr<heal::Annotation> other) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const;

//		virtual std::unique_ptr<heal::Annotation> AddInvariant(std::unique_ptr<heal::Annotation> annotation) const = 0;
//		virtual std::unique_ptr<heal::Annotation> StripInvariant(std::unique_ptr<heal::Annotation> annotation) const = 0;
//		virtual std::unique_ptr<heal::ConjunctionFormula> AddInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const = 0;
//		virtual std::unique_ptr<heal::ConjunctionFormula> StripInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const = 0;

        /** Post image establishing invariant and specification */
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Assume& cmd) const = 0;

        /** Post image establishing invariant and specification */
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Malloc& cmd) const = 0;

        /** Post image establishing invariant and specification */
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Post(const heal::Annotation& pre, const cola::Assignment& cmd) const = 0;

		/** Post image entailment check; invariant and specification may not be established/checked */
        [[nodiscard]] virtual bool PostEntailsUnchecked(const heal::Formula& pre, const cola::Assignment& cmd, const heal::Formula& post) const;

        private:
	        std::shared_ptr<SolverConfig> config;
	};

    [[nodiscard]] std::unique_ptr<Solver> MakeDefaultSolver(std::shared_ptr<SolverConfig> config);
    [[nodiscard]] std::unique_ptr<Solver> MakeDefaultSolver(std::shared_ptr<SolverConfig> config, std::unique_ptr<Encoder> encoder);

} // namespace solver

#endif