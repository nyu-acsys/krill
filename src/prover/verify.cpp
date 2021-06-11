#include "prover/verify.hpp"

#include "solver/solver.hpp"

using namespace cola;
using namespace heal;
using namespace solver;
using namespace prover;


bool prover::CheckLinearizability(const Program& program, std::unique_ptr<Solver> solver) {
    Verifier verifier(program, std::move(solver));
    verifier.VerifyProgramOrFail();
    return true;
}

bool prover::CheckLinearizability(const Program& program, std::shared_ptr<SolverConfig> config) {
    return CheckLinearizability(program, MakeDefaultSolver(std::move(config)));
}

Verifier::Verifier(const Program& program, std::unique_ptr<solver::Solver> solver_) : program(program), solver(std::move(solver_)), insideAtomic(false) {}

void Verifier::PerformStep(const std::function<std::unique_ptr<heal::Annotation>(std::unique_ptr<heal::Annotation>)>& transformer) {
    for (auto& annotation : current) {
        annotation = transformer(std::move(annotation));
    }
}

void Verifier::PerformStep(const std::function<solver::PostImage(std::unique_ptr<heal::Annotation>)>& transformer) {
    decltype(current) newCurrent;
    for (auto& annotation : current) {
        auto postImage = transformer(std::move(annotation));
        std::move(postImage.posts.begin(), postImage.posts.end(), std::back_inserter(newCurrent));
        AddNewInterference(std::move(postImage.effects));
    }
    current = std::move(newCurrent);
}

void Verifier::ApplyInterference() {
    std::cerr << "WARNING: interference skipped" << std::endl;
    return; // TODO important: re-enable <<<<<<<<<<<=====================================================================================||||||||||||
    current = solver->MakeStable(std::move(current), interference); // TODO: solve stability for all annotations in one shot?
}

void Verifier::AddNewInterference(std::deque<std::unique_ptr<solver::HeapEffect>> effects) {
    std::move(effects.begin(), effects.end(), std::back_inserter(newInterference));
}

bool Verifier::ConsolidateNewInterference() {
    // TODO: which pairs should be generated?
    auto Iterate = [this](std::function<void(std::unique_ptr<HeapEffect>&, std::unique_ptr<HeapEffect>&, std::size_t)>&& callback){
        std::size_t index = 0;
        // new vs old interference
        for (auto& it : interference)
            for (auto& other : newInterference)
                callback(it, other, index++);
        // old vs new interference
        for (auto& it : newInterference)
            for (auto& other : interference)
                callback(it, other, index++);
        // new vs new interference
        for (auto& it : newInterference)
            for (auto& other : newInterference)
                if (it.get() != other.get())
                    callback(it, other, index++);
    };

    // create implications and check
    std::deque<std::pair<const HeapEffect*, const HeapEffect*>> implications;
    Iterate([&implications](auto& effect, auto& other, auto){ implications.emplace_back(effect.get(), other.get()); });
    auto implied = solver->ComputeEffectImplications(implications);

    // prune effects that are covered by other effects
    Iterate([&](auto& premise, auto& conclusion, auto index){
        assert(implications[index].first == premise.get());
        assert(implications[index].second == conclusion.get());
        if (!implied[index]) return;
        if (!premise) return;
        conclusion.reset(nullptr);
    });
    // TODO: prune effects e which satisfy: e.post*e.context => e.pre*e.context ? They should not do anything

    // remove pruned effects
    newInterference.erase(std::remove_if(interference.begin(), interference.end(), [](auto& elem){ return !elem; }), interference.end());
    if (newInterference.empty()) return false;

    // add new effects
    // TODO: rename to avoid name clashes? but how?
    throw std::logic_error("not yet implemented: ConsolidateNewInterference");
    std::move(newInterference.begin(), newInterference.end(), std::back_inserter(interference));
    return true;
}