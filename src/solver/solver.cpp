#include "solver/solver.hpp"

#include "default/default_solver.hpp"
//#include "solver/encoder.hpp"
//#include "heal/util.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


HeapEffect::HeapEffect(std::unique_ptr<PointsToAxiom> pre_, std::unique_ptr<PointsToAxiom> post_, std::unique_ptr<Formula> context_)
        : pre(std::move(pre_)), post(std::move(post_)), context(std::move(context_)) {
    assert(pre);
    assert(post);
    assert(context);
    assert(&pre->node->Decl() == &post->node->Decl()); // TODO: throw if this is not the case?
}

PostImage::PostImage(std::unique_ptr<Annotation> post_) : post(std::move(post_)) {
    assert(post);
}

PostImage::PostImage(std::unique_ptr<Annotation> post_, std::unique_ptr<HeapEffect> effect_)
: post(std::move(post_)) {
    assert(post);
    assert(effect_);
    effects.push_back(std::move(effect_));
}

PostImage::PostImage(std::unique_ptr<Annotation> post_, std::deque<std::unique_ptr<HeapEffect>> effects_)
: post(std::move(post_)), effects(std::move(effects_)) {
    assert(post);
    for (const auto& effect : effects) assert(effect);
}

Solver::Solver(std::shared_ptr<SolverConfig> config_) : config(std::move(config_)) {
    assert(config);
}

std::unique_ptr<Annotation> Solver::Join(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) const {
    std::vector<std::unique_ptr<Annotation>> vector;
    vector.push_back(std::move(annotation));
    vector.push_back(std::move(other));
    return Join(std::move(vector));
}

std::unique_ptr<Annotation> Solver::Join(std::deque<std::unique_ptr<Annotation>> annotations) const {
    // TODO: elide this move by deleting either the vector or the deque member function?
    std::vector<std::unique_ptr<Annotation>> vector;
    vector.reserve(annotations.size());
    std::move(annotations.begin(), annotations.end(), std::back_inserter(vector));
    return Join(std::move(vector));
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
