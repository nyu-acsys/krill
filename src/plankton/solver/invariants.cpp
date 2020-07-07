#include "plankton/solver/solverimpl.hpp"

using namespace cola;
using namespace plankton;



std::unique_ptr<Annotation> SolverImpl::AddInvariant(std::unique_ptr<Annotation> annotation) const {
	annotation->now = plankton::conjoin(std::move(annotation->now), plankton::copy(*instantiatedInvariants.back()));
	return annotation;
}

std::unique_ptr<Annotation> SolverImpl::StripInvariant(std::unique_ptr<Annotation> annotation) const {
	const auto& invariant = *instantiatedInvariants.back();

	auto source = std::move(annotation->now->conjuncts);
	annotation->now->conjuncts.clear();

	for (auto& conjunct : source) {
		if (!plankton::syntactically_contains_conjunct(invariant, *conjunct)) {
			annotation->now->conjuncts.push_back(std::move(conjunct));
		}
	}

	return annotation;
}
