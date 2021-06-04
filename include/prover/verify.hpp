#pragma once
#ifndef PROVER_VERIFY
#define PROVER_VERIFY


#include <deque>
#include <vector>
#include <memory>
#include <exception>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"
#include "solver/solver.hpp"


namespace prover {

	struct Verifier final : public cola::BaseVisitor {
        explicit Verifier(const cola::Program& program, std::unique_ptr<solver::Solver> solver);
        void VerifyProgramOrFail();

        void visit(const cola::Sequence& node) override;
        void visit(const cola::Scope& node) override;
        void visit(const cola::Atomic& node) override;
        void visit(const cola::Choice& node) override;
        void visit(const cola::IfThenElse& node) override;
        void visit(const cola::Loop& node) override;
        void visit(const cola::While& node) override;
        void visit(const cola::DoWhile& node) override;
        void visit(const cola::Skip& node) override;
        void visit(const cola::Break& node) override;
        void visit(const cola::Continue& node) override;
        void visit(const cola::Assume& node) override;
        void visit(const cola::Assert& node) override;
        void visit(const cola::Return& node) override;
        void visit(const cola::Malloc& node) override;
        void visit(const cola::Assignment& node) override;
        void visit(const cola::Macro& node) override;
        void visit(const cola::CompareAndSwap& node) override;
        void visit(const cola::Program& prog) override;

    private:
        const cola::Program& program;
        std::unique_ptr<solver::Solver> solver;
        std::deque<std::unique_ptr<solver::HeapEffect>> interference;
        std::deque<std::unique_ptr<solver::HeapEffect>> newInterference;
        std::deque<std::unique_ptr<heal::Annotation>> current;
        std::deque<std::unique_ptr<heal::Annotation>> breaking;
        std::deque<std::pair<std::unique_ptr<heal::Annotation>, const cola::Return*>> returning;
        bool insideAtomic;

        void HandleInterfaceFunction(const cola::Function& function);
        void HandleLoop(const cola::ConditionalLoop& loop);

        void ApplyInterference();
        bool ConsolidateNewInterference();
        void AddNewInterference(std::deque<std::unique_ptr<solver::HeapEffect>> effects);
        void PerformStep(const std::function<std::unique_ptr<heal::Annotation>(std::unique_ptr<heal::Annotation>)>& transformer);
        void PerformStep(const std::function<solver::PostImage(std::unique_ptr<heal::Annotation>)>& transformer);
	};


    bool CheckLinearizability(const cola::Program& program, std::unique_ptr<solver::Solver> solver);
    bool CheckLinearizability(const cola::Program& program, std::shared_ptr<solver::SolverConfig> config);

} // namespace prover

#endif