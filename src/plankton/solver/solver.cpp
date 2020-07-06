#include "plankton/solver.hpp"

#include "plankton/solver/solverimpl.hpp"

using namespace cola;
using namespace plankton;


Solver::Solver(PostConfig config_) : config(std::move(config_)) {
	assert(config.flowDomain);
	assert(config.logicallyContainsKey);
	assert(config.invariant);
}

bool Solver::ImpliesFalse(const Formula& formula) const {
	return MakeImplicationChecker(formula)->ImpliesFalse();
}

bool Solver::Implies(const Formula& formula, const Formula& implied) const {
	return MakeImplicationChecker(formula)->Implies(implied);
}

bool Solver::Implies(const Formula& formula, const Expression& implied) const {
	return MakeImplicationChecker(formula)->Implies(implied);
}

bool Solver::ImpliesNonNull(const Formula& formula, const Expression& nonnull) const {
	return MakeImplicationChecker(formula)->ImpliesNonNull(nonnull);
}

std::unique_ptr<Annotation> Solver::Join(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) const {
	std::vector<std::unique_ptr<Annotation>> vec;
	vec.push_back(std::move(annotation));
	vec.push_back(std::move(other));
	return Join(std::move(vec));
}

bool Solver::PostEntails(const Formula& pre, const Assume& cmd, const Formula& post) const {
	auto post_annotation = this->Post(pre, cmd);
	return Implies(*post_annotation, post);
}

bool Solver::PostEntails(const Formula& pre, const Malloc& cmd, const Formula& post) const {
	auto post_annotation = this->Post(pre, cmd);
	return Implies(*post_annotation, post);
}

bool Solver::PostEntails(const Formula& pre, const Assignment& cmd, const Formula& post) const {
	auto post_annotation = this->Post(pre, cmd);
	return Implies(*post_annotation, post);
}

void Solver::LeaveScope(std::size_t amount) {
	while (amount > 0) {
		LeaveScope();
		--amount;
	}
}
