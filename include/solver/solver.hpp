#pragma once
#ifndef SOLVER_SOLVER
#define SOLVER_SOLVER


#include <memory>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "config.hpp"


namespace solver {

    struct HeapEffect {
        std::unique_ptr<heal::PointsToAxiom> pre; // resources before update
        std::unique_ptr<heal::PointsToAxiom> post; // resources after update
        std::unique_ptr<heal::Formula> context; // required but unaltered resources (not a frame!)

        explicit HeapEffect(std::unique_ptr<heal::PointsToAxiom> pre, std::unique_ptr<heal::PointsToAxiom> post, std::unique_ptr<heal::Formula> context);
    };

    struct PostImage {
        std::unique_ptr<heal::Annotation> post;
        std::deque<std::unique_ptr<HeapEffect>> effects;

        explicit PostImage(std::unique_ptr<heal::Annotation> post);
        explicit PostImage(std::unique_ptr<heal::Annotation> post, std::unique_ptr<HeapEffect> effect);
        explicit PostImage(std::unique_ptr<heal::Annotation> post, std::deque<std::unique_ptr<HeapEffect>> effects);
    };

	/** A solver for post images. The solver works relative to an invariant that it implicitly assumes and enforces.
	  */
	struct Solver {
	    enum Result { YES, NO, UNKNOWN };

		explicit Solver(std::shared_ptr<SolverConfig> config);
        virtual ~Solver() = default;

        // TODO: should take references, not consume the original?
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Join(std::deque<std::unique_ptr<heal::Annotation>> annotations) const;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> Join(std::unique_ptr<heal::Annotation> annotation, std::unique_ptr<heal::Annotation> other) const;

        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> PostEnterScope(std::unique_ptr<heal::Annotation> pre, const cola::Program& scope) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> PostEnterScope(std::unique_ptr<heal::Annotation> pre, const cola::Function& scope) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> PostEnterScope(std::unique_ptr<heal::Annotation> pre, const cola::Scope& scope) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> PostLeaveScope(std::unique_ptr<heal::Annotation> pre, const cola::Program& scope) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> PostLeaveScope(std::unique_ptr<heal::Annotation> pre, const cola::Function& scope) const = 0;
        [[nodiscard]] virtual std::unique_ptr<heal::Annotation> PostLeaveScope(std::unique_ptr<heal::Annotation> pre, const cola::Scope& scope) const = 0;

        [[nodiscard]] virtual PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Assume& cmd) const = 0;
        [[nodiscard]] virtual PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Malloc& cmd) const = 0;
        [[nodiscard]] virtual PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd) const = 0;

        // TODO: streamline; are those function at the right place, with the right signature?
        [[nodiscard]] virtual const heal::EqualsToAxiom* GetVariableResource(const cola::VariableDeclaration& decl, const heal::LogicObject& object) const = 0;
        [[nodiscard]] virtual std::optional<bool> GetBoolValue(const cola::Expression& expr, const heal::LogicObject& object) const = 0;
        [[nodiscard]] virtual Result Implies(const heal::Annotation& premise, const heal::Annotation& conclusion) const = 0;
        [[nodiscard]] virtual std::vector<bool> ComputeEffectImplications(const std::deque<std::pair<const HeapEffect*, const HeapEffect*>>& implications) const = 0;
        [[nodiscard]] virtual std::deque<std::unique_ptr<heal::Annotation>> MakeStable(std::deque<std::unique_ptr<heal::Annotation>> annotations, const std::deque<std::unique_ptr<HeapEffect>>& interferences) const = 0;

	    protected:
            [[nodiscard]] const SolverConfig& Config() const { return *config; }

        private:
	        std::shared_ptr<SolverConfig> config;
	};

    [[nodiscard]] std::unique_ptr<Solver> MakeDefaultSolver(std::shared_ptr<SolverConfig> config);

} // namespace solver

#endif