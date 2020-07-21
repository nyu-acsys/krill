#include "plankton/solver/solverimpl.hpp"

using namespace cola;
using namespace plankton;


inline std::unique_ptr<ConjunctionFormula> Add(std::unique_ptr<ConjunctionFormula> formula, const ConjunctionFormula& add) {
	return plankton::conjoin(std::move(formula), plankton::copy(add));
}

inline std::unique_ptr<Annotation> Add(std::unique_ptr<Annotation> annotation, const ConjunctionFormula& add) {
	annotation->now = Add(std::move(annotation->now), add);
	return annotation;
}


std::unique_ptr<ConjunctionFormula> SolverImpl::AddRules(std::unique_ptr<ConjunctionFormula> formula) const {
	return Add(std::move(formula), GetInstantiatedRules());
}

std::unique_ptr<Annotation> SolverImpl::AddRules(std::unique_ptr<Annotation> annotation) const {
	return Add(std::move(annotation), GetInstantiatedRules());
}


std::unique_ptr<ConjunctionFormula> SolverImpl::AddInvariant(std::unique_ptr<ConjunctionFormula> formula) const {
	return Add(std::move(formula), GetInstantiatedInvariant());
}

std::unique_ptr<Annotation> SolverImpl::AddInvariant(std::unique_ptr<Annotation> annotation) const {
	return Add(std::move(annotation), GetInstantiatedInvariant());
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

inline std::unique_ptr<ConjunctionFormula> Strip(std::unique_ptr<ConjunctionFormula> formula, const ConjunctionFormula& remove) {
	auto result = std::make_unique<ConjunctionFormula>();
	for (auto& conjunct : formula->conjuncts) {
		if (!plankton::syntactically_contains_conjunct(remove, *conjunct)) {
			result->conjuncts.push_back(std::move(conjunct));
		}
	}
	return result;
}

inline std::unique_ptr<Annotation> Strip(std::unique_ptr<Annotation> annotation, const ConjunctionFormula& remove) {
	annotation->now = Strip(std::move(annotation->now), remove);
	return annotation;
}


std::unique_ptr<ConjunctionFormula> SolverImpl::StripRules(std::unique_ptr<ConjunctionFormula> formula) const {
	return Strip(std::move(formula), GetInstantiatedRules());
}

std::unique_ptr<Annotation> SolverImpl::StripRules(std::unique_ptr<Annotation> annotation) const {
	return Strip(std::move(annotation), GetInstantiatedRules());
}


std::unique_ptr<ConjunctionFormula> SolverImpl::StripInvariant(std::unique_ptr<ConjunctionFormula> formula) const {
	return Strip(std::move(formula), GetInstantiatedInvariant());
}

std::unique_ptr<Annotation> SolverImpl::StripInvariant(std::unique_ptr<Annotation> annotation) const {
	return Strip(std::move(annotation), GetInstantiatedInvariant());
}
