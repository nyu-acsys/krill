#pragma once
#ifndef PLANKTON_ENGINE_PROOF_HPP
#define PLANKTON_ENGINE_PROOF_HPP

#include <deque>
#include <memory>
#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/config.hpp"
#include "engine/solver.hpp"

namespace plankton {

    struct ProofGenerator final : public BaseProgramVisitor {
        explicit ProofGenerator(const Program& program, const SolverConfig& config);
        void GenerateProof();

        // TODO: can we make those inaccessible for callers generating a proof?
        void Visit(const Sequence& object) override;
        void Visit(const Scope& object) override;
        void Visit(const Atomic& object) override;
        void Visit(const Choice& object) override;
        void Visit(const UnconditionalLoop& object) override;
        void Visit(const Skip& object) override;
        void Visit(const Break& object) override;
        void Visit(const Assume& object) override;
        void Visit(const Assert& object) override;
        void Visit(const Return& object) override;
        void Visit(const Malloc& object) override;
        void Visit(const Macro& object) override;
        void Visit(const VariableAssignment& object) override;
        void Visit(const MemoryRead& object) override;
        void Visit(const MemoryWrite& object) override;
        void Visit(const Function& object) override;
        void Visit(const Program& object) override;

    private:
        const Program& program;
        Solver solver;
        std::deque<std::unique_ptr<HeapEffect>> newInterference;
        std::deque<std::unique_ptr<Annotation>> current;
        std::deque<std::unique_ptr<Annotation>> breaking;
        std::deque<std::pair<std::unique_ptr<Annotation>, const Return*>> returning;
        bool insideAtomic;
    
        void HandleInterfaceFunction(const Function& function);
        void HandleMacroProlog(const Macro& macro);
        void HandleMacroEpilog(const Macro& macro);

        void MakeInterferenceStable(const Statement& after);
        void AddNewInterference(std::deque<std::unique_ptr<HeapEffect>> effects);
        bool ConsolidateNewInterference();
        
        void ApplyTransformer(const std::function<std::unique_ptr<Annotation>(std::unique_ptr<Annotation>)>& transformer);
        void ApplyTransformer(const std::function<PostImage(std::unique_ptr<Annotation>)>& transformer);
    };

} // namespace plankton

#endif //PLANKTON_ENGINE_PROOF_HPP
