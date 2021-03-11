#include "default_solver.hpp"

using namespace heal;
using namespace solver;

DefaultSolver::DefaultSolver(std::shared_ptr<SolverConfig> config, std::unique_ptr<Encoder> encoder) : Solver(std::move(config)), encoder(std::move(encoder)) {
}

std::unique_ptr<ImplicationChecker> DefaultSolver::MakeImplicationChecker() const {
    return encoder->MakeImplicationChecker();
}

std::unique_ptr<Annotation> DefaultSolver::Join(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) const {
    std::vector<std::unique_ptr<Annotation>> tmp;
    tmp.push_back(std::move(annotation));
    tmp.push_back((std::move(other)));
    return Join(std::move(tmp));
}

std::unique_ptr<Annotation> DefaultSolver::Join(std::vector<std::unique_ptr<Annotation>> annotations) const {
    // TODO: implement
    throw std::logic_error("not yet implemented");
}

std::unique_ptr<Annotation> DefaultSolver::Post(const Annotation &pre, const cola::Assume &cmd) const {
    // TODO: implement
    throw std::logic_error("not yet implemented");
}

std::unique_ptr<Annotation> DefaultSolver::Post(const Annotation &pre, const cola::Malloc &cmd) const {
    // TODO: implement
    throw std::logic_error("not yet implemented");
}

std::unique_ptr<Annotation> DefaultSolver::Post(const Annotation &pre, const cola::Assignment &cmd) const {
    // TODO: implement
    throw std::logic_error("not yet implemented");
}

bool DefaultSolver::PostEntailsUnchecked(const Formula& pre, const cola::Assignment& cmd, const Formula& post) const {
    // TODO: implement
    throw std::logic_error("not yet implemented");
}