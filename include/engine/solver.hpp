#pragma once
#ifndef PLANKTON_ENGINE_SOLVER_HPP
#define PLANKTON_ENGINE_SOLVER_HPP

#include <deque>
#include <memory>
#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/config.hpp"

namespace plankton {

    struct HeapEffect final {
        std::unique_ptr<PointsToAxiom> pre; // memory before update
        std::unique_ptr<PointsToAxiom> post; // memory after update
        std::unique_ptr<Formula> context; // required but unaltered resources (not a frame!)
    
        explicit HeapEffect(std::unique_ptr<PointsToAxiom> pre, std::unique_ptr<PointsToAxiom> post,
                            std::unique_ptr<Formula> context);
    };

    struct PostImage final {
        std::deque<std::unique_ptr<Annotation>> annotations;
        std::deque<std::unique_ptr<HeapEffect>> effects;

        explicit PostImage();
        explicit PostImage(std::unique_ptr<Annotation> post);
        explicit PostImage(std::deque<std::unique_ptr<Annotation>> posts);
        explicit PostImage(std::unique_ptr<Annotation> post,
                           std::deque<std::unique_ptr<HeapEffect>> effects);
        explicit PostImage(std::deque<std::unique_ptr<Annotation>> posts,
                           std::deque<std::unique_ptr<HeapEffect>> effects);
    };

    struct Solver final {
        explicit Solver(const SolverConfig& config);

        [[nodiscard]] std::unique_ptr<Annotation> PostEnter(std::unique_ptr<Annotation> pre, const Program& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostEnter(std::unique_ptr<Annotation> pre, const Function& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostEnter(std::unique_ptr<Annotation> pre, const Scope& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostLeave(std::unique_ptr<Annotation> pre, const Program& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostLeave(std::unique_ptr<Annotation> pre, const Function& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostLeave(std::unique_ptr<Annotation> pre, const Scope& scope) const;

        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const;
        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const Malloc& cmd) const;
        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const Assignment& cmd) const;
        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const ParallelAssignment& cmd) const;

        [[nodiscard]] bool Implies(const Annotation& premise, const Annotation& conclusion) const;

        [[nodiscard]] std::unique_ptr<Annotation> Join(std::vector<std::unique_ptr<Annotation>> annotations) const;
    
        [[nodiscard]] std::unique_ptr<Annotation> TryAddFulfillment(std::unique_ptr<Annotation> annotation) const;

        [[nodiscard]] std::unique_ptr<Annotation> MakeInterferenceStable(std::unique_ptr<Annotation> annotation) const;
        bool AddInterference(std::deque<std::unique_ptr<HeapEffect>> interference);
        
//        [[nodiscard]] PostImage PostVariableUpdate(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd, const cola::VariableExpression& lhs) const;
//        [[nodiscard]] PostImage PostMemoryUpdate(std::unique_ptr<heal::Annotation> pre, const MultiUpdate& update) const;
//        [[nodiscard]] std::deque<std::unique_ptr<heal::Annotation>> MakeInterferenceStable(std::deque<std::unique_ptr<heal::Annotation>> annotations) const;
        // i don't like those (here)
//        [[nodiscard]] const heal::EqualsToAxiom* GetVariableResource(const cola::VariableDeclaration& decl, const heal::LogicObject& object) const override;
//        [[nodiscard]] std::optional<bool> GetBoolValue(const cola::Expression& expr, const heal::LogicObject& object) const override;
//        [[nodiscard]] std::vector<bool> ComputeEffectImplications(const std::deque<std::pair<const HeapEffect*, const HeapEffect*>>& implications) const override;
//        [[nodiscard]] std::unique_ptr<heal::Annotation> Interpolate(std::unique_ptr<heal::Annotation> annotation, const std::deque<std::unique_ptr<HeapEffect>>& interferences) const;
//        [[nodiscard]] std::unique_ptr<heal::Annotation> TryFindBetterHistories(std::unique_ptr<heal::Annotation> annotation, const std::deque<std::unique_ptr<HeapEffect>>& interferences) const override;

        private:
            const SolverConfig& config;
            std::deque<std::unique_ptr<HeapEffect>> interference;
    };

} // namespace plankton

#endif //PLANKTON_ENGINE_SOLVER_HPP
