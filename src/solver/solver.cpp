#include "solver/solver.hpp"

#include "default_solver.hpp"
#include "solver/encoder.hpp"
#include "heal/util.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


std::unique_ptr<ImplicationChecker> Solver::MakeImplicationChecker(const Formula& premise) const {
    auto result = MakeImplicationChecker();
    result->AddPremise(premise);
    return result;
}

std::unique_ptr<heal::Annotation> Solver::Join(std::vector<std::unique_ptr<Annotation>> annotations) const {
    auto result = Annotation::True();
    while (!annotations.empty()) {
        result = Join(std::move(result), std::move(annotations.back()));
        annotations.pop_back();
    }
    return result;
}

bool Solver::PostEntailsUnchecked(const Formula& pre, const Assignment& cmd, const Formula& post) const {
    Annotation dummy(heal::Copy(pre));
    auto computedPost = Post(dummy, cmd);
    auto checker = MakeImplicationChecker(*computedPost->now);
    return checker->Implies(post);
}

std::unique_ptr<Solver> solver::MakeDefaultSolver(std::shared_ptr<SolverConfig> config) {
    return MakeDefaultSolver(config, solver::MakeDefaultEncoder(std::move(config)));
}

std::unique_ptr<Solver> solver::MakeDefaultSolver(std::shared_ptr<SolverConfig> config, std::unique_ptr<Encoder> encoder) {
    return std::make_unique<DefaultSolver>(std::move(config), std::move(encoder));
}
