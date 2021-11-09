#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;


template<typename C>
static inline std::function<PostImage(std::unique_ptr<Annotation>)> MakePostTransformer(const C& cmd, const Solver& solver) {
    return [&cmd, &solver](auto annotation){ return solver.Post(std::move(annotation), cmd); };
}


void ProofGenerator::Visit(const Skip& /*cmd*/) {
    /* do nothing */
}

void ProofGenerator::Visit(const Break& /*cmd*/) {
    DEBUG(" BREAKING with (" << current.size() << ") ") for (const auto& elem : current) DEBUG(*elem) DEBUG(std::endl)
    MoveInto(std::move(current), breaking);
    current.clear();
}

void ProofGenerator::Visit(const Fail& /*cmd*/) {
    for (const auto& annotation : current) {
        if (solver.IsUnsatisfiable(*annotation)) continue;
        throw std::logic_error("Program potentially fails / violates an assertion.");
    }
    current.clear();
}

void ProofGenerator::Visit(const Assume& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const Malloc& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const VariableAssignment& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const MemoryWrite& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const Return& cmd) {
    DEBUG(" RETURNING " << cmd << " with (" << current.size() << ") ") for (const auto& elem : current) DEBUG(*elem) DEBUG(std::endl)
    // pointer accessibility is checked when returned values/variables are used
    for (auto& annotation : current) returning.emplace_back(std::move(annotation), &cmd);
    current.clear();
}
