#pragma once
#ifndef PLANKTON_ENGINE_SOLVER_HPP
#define PLANKTON_ENGINE_SOLVER_HPP

#include <deque>
#include <memory>
#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "config.hpp"
#include "static.hpp"

namespace plankton {

    struct HeapEffect final {
        std::unique_ptr<SharedMemoryCore> pre; // memory before update
        std::unique_ptr<SharedMemoryCore> post; // memory after update
        std::unique_ptr<Formula> context; // required but unaltered resources (not a frame!)
    
        explicit HeapEffect(std::unique_ptr<SharedMemoryCore> pre, std::unique_ptr<SharedMemoryCore> post,
                            std::unique_ptr<Formula> context);
    };

    struct PostImage final {
        std::deque<std::unique_ptr<Annotation>> annotations;
        std::deque<std::unique_ptr<HeapEffect>> effects;

        explicit PostImage();
        explicit PostImage(std::unique_ptr<Annotation> post);
        explicit PostImage(std::deque<std::unique_ptr<Annotation>> posts);
        explicit PostImage(std::unique_ptr<Annotation> post, std::deque<std::unique_ptr<HeapEffect>> effects);
        explicit PostImage(std::deque<std::unique_ptr<Annotation>> posts, std::deque<std::unique_ptr<HeapEffect>> effects);
    };

    struct FutureSuggestion {
        std::unique_ptr<Guard> guard;
        std::unique_ptr<MemoryWrite> command;

        explicit FutureSuggestion(std::unique_ptr<MemoryWrite> command);
        explicit FutureSuggestion(std::unique_ptr<MemoryWrite> command, std::unique_ptr<Guard> guard);
    };

    struct Solver final {
        explicit Solver(const Program& program, const SolverConfig& config);

        [[nodiscard]] std::unique_ptr<Annotation> PostEnter(std::unique_ptr<Annotation> pre, const Program& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostEnter(std::unique_ptr<Annotation> pre, const Function& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostEnter(std::unique_ptr<Annotation> pre, const Scope& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostLeave(std::unique_ptr<Annotation> pre, const Function& scope) const;
        [[nodiscard]] std::unique_ptr<Annotation> PostLeave(std::unique_ptr<Annotation> pre, const Scope& scope) const;

        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const;
        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const Malloc& cmd) const;
        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const VariableAssignment& cmd) const;
        [[nodiscard]] PostImage Post(std::unique_ptr<Annotation> pre, const MemoryWrite& cmd, bool useFuture = true) const;
        
        [[nodiscard]] std::unique_ptr<Annotation> Join(std::deque<std::unique_ptr<Annotation>> annotations) const;
        [[nodiscard]] std::unique_ptr<Annotation> TryAddFulfillment(std::unique_ptr<Annotation> annotation) const;

        bool AddInterference(std::deque<std::unique_ptr<HeapEffect>> interference);
        [[nodiscard]] std::unique_ptr<Annotation> MakeInterferenceStable(std::unique_ptr<Annotation> annotation) const;

        [[nodiscard]] bool IsUnsatisfiable(const Annotation& annotation) const;
        [[nodiscard]] bool Implies(const Annotation& premise, const Annotation& conclusion) const;

        [[nodiscard]] PostImage ImproveFuture(std::unique_ptr<Annotation> annotation, const FutureSuggestion& target) const;
        // TODO: the Solver should not decide when to call ImprovePast, should be done by the proof generating entity // TODO: past suggestions
        
        private:
            const SolverConfig& config;
            DataFlowAnalysis dataFlow;
            std::deque<std::unique_ptr<HeapEffect>> interference;
            
            void PrepareAccess(Annotation& annotation, const Command& command) const;
            void ImprovePast(Annotation& annotation) const;
            void PrunePast(Annotation& annotation) const; // TODO: should not be exposed ~~> only used by ImprovePast?
    };
    
    
    std::ostream& operator<<(std::ostream& out, const HeapEffect& object);

} // namespace plankton

#endif //PLANKTON_ENGINE_SOLVER_HPP
