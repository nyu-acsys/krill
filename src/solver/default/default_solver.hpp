#pragma once
#ifndef SOLVER_DEFAULTSOLVER
#define SOLVER_DEFAULTSOLVER

#include "solver/solver.hpp"
#include "eval.hpp"

namespace solver {

    struct MultiUpdate {
        std::vector<std::pair<const cola::Dereference*, const cola::SimpleExpression*>> updates;
        explicit MultiUpdate(const cola::Assignment& assignment);
        explicit MultiUpdate(const cola::ParallelAssignment& assignment);
        [[nodiscard]] std::vector<std::pair<const cola::Dereference*, const cola::SimpleExpression*>>::const_iterator begin() const { return updates.begin(); }
        [[nodiscard]] std::vector<std::pair<const cola::Dereference*, const cola::SimpleExpression*>>::const_iterator end() const { return updates.end(); }
    };

    struct DefaultSolver final : public Solver {
        explicit DefaultSolver(std::shared_ptr<SolverConfig> config) : Solver(std::move(config)) {
            // TODO: solver relies on decreasing laminar flow!?
        }

        [[nodiscard]] std::unique_ptr<heal::Annotation> Join(std::vector<std::unique_ptr<heal::Annotation>> annotations) const override;

        [[nodiscard]] std::unique_ptr<heal::Annotation> PostEnterScope(std::unique_ptr<heal::Annotation> pre, const cola::Program& scope) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> PostEnterScope(std::unique_ptr<heal::Annotation> pre, const cola::Function& scope) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> PostEnterScope(std::unique_ptr<heal::Annotation> pre, const cola::Scope& scope) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> PostLeaveScope(std::unique_ptr<heal::Annotation> pre, const cola::Program& scope) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> PostLeaveScope(std::unique_ptr<heal::Annotation> pre, const cola::Function& scope) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> PostLeaveScope(std::unique_ptr<heal::Annotation> pre, const cola::Scope& scope) const override;

        [[nodiscard]] PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Assume& cmd) const override;
        [[nodiscard]] PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Malloc& cmd) const override;
        [[nodiscard]] PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd) const override;
        [[nodiscard]] PostImage Post(std::unique_ptr<heal::Annotation> pre, const cola::ParallelAssignment& cmd) const override;

        [[nodiscard]] PostImage PostVariableUpdate(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd, const cola::VariableExpression& lhs) const;
        [[nodiscard]] PostImage PostMemoryUpdate(std::unique_ptr<heal::Annotation> pre, const MultiUpdate& update) const;

        // i don't like those (here)
        [[nodiscard]] Result Implies(const heal::Annotation& premise, const heal::Annotation& conclusion) const override;
        [[nodiscard]] const heal::EqualsToAxiom* GetVariableResource(const cola::VariableDeclaration& decl, const heal::LogicObject& object) const override;
        [[nodiscard]] std::optional<bool> GetBoolValue(const cola::Expression& expr, const heal::LogicObject& object) const override;
        [[nodiscard]] std::vector<bool> ComputeEffectImplications(const std::deque<std::pair<const HeapEffect*, const HeapEffect*>>& implications) const override;
        [[nodiscard]] std::deque<std::unique_ptr<heal::Annotation>> MakeStable(std::deque<std::unique_ptr<heal::Annotation>> annotations, const std::deque<std::unique_ptr<HeapEffect>>& interferences) const override;
        [[nodiscard]] std::unique_ptr<heal::Annotation> Interpolate(std::unique_ptr<heal::Annotation> annotation, const std::deque<std::unique_ptr<HeapEffect>>& interferences) const;

        // internal helpers
        [[nodiscard]] std::unique_ptr<heal::Annotation> TryFindBetterHistories(std::unique_ptr<heal::Annotation> annotation) const;
    };

}

#endif
