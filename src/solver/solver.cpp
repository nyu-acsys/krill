#include "solver/solver.hpp"

#include "default/default_solver.hpp"
//#include "solver/encoder.hpp"
//#include "heal/util.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


Effect::Effect(std::unique_ptr<Annotation> change_, std::unique_ptr<Annotation> context_, const cola::Assignment &command)
        : change(std::move(change_)), context(std::move(context_)), command(command) {
    assert(change);
    assert(context);
}

Effect::Effect(std::unique_ptr<Formula> change_, std::unique_ptr<Formula> context_, const cola::Assignment& command)
        : change(std::make_unique<Annotation>()), context(std::make_unique<Annotation>()), command(command) {
    change->now->conjuncts.push_back(std::move(change_));
    context->now->conjuncts.push_back(std::move(context_));
    heal::Simplify(*change->now);
    heal::Simplify(*context->now);
}

Solver::Solver(std::shared_ptr<SolverConfig> config) : config(std::move(config)) {
    assert(config);
}

std::unique_ptr<Annotation> Solver::Join(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) const {
    std::vector<std::unique_ptr<Annotation>> vector;
    vector.push_back(std::move(annotation));
    vector.push_back(std::move(other));
    return Join(std::move(vector));
}

std::unique_ptr<Annotation> Solver::Post(const Annotation& pre, const Assume& cmd) const {
    return Post(heal::Copy(pre), cmd);
}

std::unique_ptr<Annotation> Solver::Post(const Annotation& pre, const Malloc& cmd) const {
    return Post(heal::Copy(pre), cmd);
}

std::pair<std::unique_ptr<Annotation>, std::unique_ptr<Effect>> Solver::Post(const Annotation& pre, const Assignment& cmd) const {
    return Post(heal::Copy(pre), cmd);
}

std::unique_ptr<Annotation> Solver::MakeStable(const Annotation& pre, const Effect& interference) const {
    return MakeStable(heal::Copy(pre), interference);
}

std::unique_ptr<Solver> solver::MakeDefaultSolver(std::shared_ptr<SolverConfig> config) {
    return std::make_unique<DefaultSolver>(std::move(config));
}

//std::unique_ptr<ImplicationChecker> Solver::MakeImplicationChecker(const Formula& premise) const {
//    auto result = MakeImplicationChecker();
//    result->AddPremise(premise);
//    return result;
//}

//bool Solver::PostEntailsUnchecked(const Formula& pre, const Assignment& cmd, const Formula& post) const {
//    Annotation dummy(heal::MakeConjunction(heal::Copy(pre)));
//    auto computedPost = Post(dummy, cmd);
//    auto checker = MakeImplicationChecker(*computedPost->now);
//    return checker->Implies(post);
//}

//std::unique_ptr<Solver> solver::MakeDefaultSolver(std::shared_ptr<SolverConfig> config, std::unique_ptr<Encoder> encoder) {
//    return std::make_unique<DefaultSolver>(std::move(config), std::move(encoder));
//}
