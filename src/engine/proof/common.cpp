#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


ProofGenerator::ProofGenerator(const Program& program, const SolverConfig& config)
        : program(program), solver(config), insideAtomic(false) {}

void ProofGenerator::ApplyTransformer(
        const std::function<std::unique_ptr<Annotation>(std::unique_ptr<Annotation>)>& transformer) {
    if (current.empty()) return;
    for (auto& annotation : current) {
        annotation = transformer(std::move(annotation));
    }
}

void ProofGenerator::ApplyTransformer(const std::function<PostImage(std::unique_ptr<Annotation>)>& transformer) {
    if (current.empty()) return;
    decltype(current) newCurrent;
    for (auto& annotation : current) {
        auto postImage = transformer(std::move(annotation));
        MoveInto(std::move(postImage.annotations), newCurrent);
        AddNewInterference(std::move(postImage.effects));
    }
    current = std::move(newCurrent);
}

void ProofGenerator::AddNewInterference(std::deque<std::unique_ptr<HeapEffect>> effects) {
    MoveInto(effects, newInterference);
}

bool ProofGenerator::ConsolidateNewInterference() {
    auto result = solver.AddInterference(std::move(newInterference));
    newInterference.clear();
    return result;
}

void ProofGenerator::MakeInterferenceStable(const Statement& after) {
    if (insideAtomic) return;
    if (current.empty()) return;
    if (plankton::IsRightMover(after)) return;
    ApplyTransformer([this](auto annotation){
        return solver.MakeInterferenceStable(std::move(annotation));
    });
}
