#include "plankton/solver/solverimpl.hpp"

using namespace cola;
using namespace plankton;



std::unique_ptr<ConjunctionFormula> SolverImpl::AddInvariant(std::unique_ptr<ConjunctionFormula> formula) const {
	return plankton::conjoin(std::move(formula), plankton::copy(GetInstantiatedInvariant()));
}

std::unique_ptr<Annotation> SolverImpl::AddInvariant(std::unique_ptr<Annotation> annotation) const {
	annotation->now = AddInvariant(std::move(annotation->now));
	return annotation;
}

std::unique_ptr<ConjunctionFormula> SolverImpl::StripInvariant(std::unique_ptr<ConjunctionFormula> formula) const {
	const auto& invariant = GetInstantiatedInvariant();

	auto result = std::make_unique<ConjunctionFormula>();
	for (auto& conjunct : formula->conjuncts) {
		if (!plankton::syntactically_contains_conjunct(invariant, *conjunct)) {
			result->conjuncts.push_back(std::move(conjunct));
		}
	}

	return result;
}

std::unique_ptr<Annotation> SolverImpl::StripInvariant(std::unique_ptr<Annotation> annotation) const {
	annotation->now = StripInvariant(std::move(annotation->now));
	return annotation;
}