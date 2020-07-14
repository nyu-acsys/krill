#include "plankton/solver/solverimpl.hpp"

#include "plankton/error.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;



SolverImpl::SolverImpl(PostConfig config_) : Solver(std::move(config_)), encoder(config) {
}

std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker(const Formula& formula) const {
	return std::make_unique<ImplicationCheckerImpl>(encoder, formula);
}

std::unique_ptr<ConjunctionFormula> SolverImpl::PruneNonCandidates(std::unique_ptr<ConjunctionFormula> formula) const {
	formula = plankton::remove_conjuncts_if(std::move(formula), [this](const auto& conjunct) {
		return !plankton::syntactically_contains_conjunct(GetCandidates(), conjunct);
	});
	return formula;
}

std::unique_ptr<ConjunctionFormula> SolverImpl::ExtendExhaustively(std::unique_ptr<ConjunctionFormula> formula, bool removeNonCandidates) const {
	// find conjuncts to be added
	auto checker = MakeImplicationChecker(*formula);
	auto newConjuncts = std::make_unique<ConjunctionFormula>();
	for (const auto& conjunct : GetCandidates().conjuncts) {
		if (plankton::syntactically_contains_conjunct(*formula, *conjunct)) continue;
		if (!checker->Implies(*conjunct)) continue;
		newConjuncts->conjuncts.push_back(plankton::copy(*conjunct));
	}

	// filter formula according to candidates (only if 'removeNonCandidates == true')
	if (removeNonCandidates) {
		formula = PruneNonCandidates(std::move(formula));
	}

	// done
	return plankton::conjoin(std::move(formula), std::move(newConjuncts));
}