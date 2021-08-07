#include "engine/solver.hpp"

using namespace plankton;


HeapEffect::HeapEffect(std::unique_ptr<SharedMemoryCore> before, std::unique_ptr<SharedMemoryCore> after,
                       std::unique_ptr<Formula> ctx) : pre(std::move(before)), post(std::move(after)),
                                                       context(std::move(ctx)) {
    assert(pre);
    assert(post);
    assert(pre->node->Decl() == post->node->Decl());
    assert(context);
}

//    std::deque<std::unique_ptr<Annotation>> annotations;
//    std::deque<std::unique_ptr<HeapEffect>> effects;

PostImage::PostImage() = default;

PostImage::PostImage(std::unique_ptr<Annotation> post) {
    assert(post);
    annotations.push_back(std::move(post));
}

PostImage::PostImage(std::deque<std::unique_ptr<Annotation>> posts) : annotations(std::move(posts)) {
    for (const auto& elem : annotations) assert(elem);
}

PostImage::PostImage(std::unique_ptr<Annotation> post, std::deque<std::unique_ptr<HeapEffect>> effects)
        : effects(std::move(effects)) {
    assert(post);
    annotations.push_back(std::move(post));
    for (const auto& elem : effects) assert(elem);
}

PostImage::PostImage(std::deque<std::unique_ptr<Annotation>> posts, std::deque<std::unique_ptr<HeapEffect>> effects)
        : annotations(std::move(posts)), effects(std::move(effects)) {
    for (const auto& elem : annotations) assert(elem);
    for (const auto& elem : effects) assert(elem);
}

Solver::Solver(const SolverConfig& config) : config(config) {
    // TODO: perform sanity/assumption checks on config?
}