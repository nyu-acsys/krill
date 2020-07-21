#include "plankton/solver/solverimpl.hpp"

#include "plankton/error.hpp"
#include "plankton/util.hpp"
#include "plankton/logger.hpp" // TODO: remove

using namespace cola;
using namespace plankton;


Candidate::Candidate(std::unique_ptr<ConjunctionFormula> repr_) : repr(std::move(repr_)) {
	assert(repr);
	assert(!repr->conjuncts.empty());
}

Candidate::Candidate(std::unique_ptr<SimpleFormula> candidate_) : repr(std::make_unique<ConjunctionFormula>()) {
	assert(candidate_);
	repr->conjuncts.push_back(std::move(candidate_));
}

Candidate::Candidate(std::unique_ptr<SimpleFormula> candidate, std::unique_ptr<ConjunctionFormula> implied) : Candidate(std::move(candidate)) {
	assert(implied);
	repr = plankton::conjoin(std::move(repr), std::move(implied));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


SolverImpl::SolverImpl(PostConfig config_) : Solver(std::move(config_)), encoder(config) {
}

std::unique_ptr<ImplicationCheckerImpl> SolverImpl::MakeImplicationChecker(const NowFormula& formula) const {
	return std::make_unique<ImplicationCheckerImpl>(encoder, formula);
}

std::unique_ptr<ImplicationCheckerImpl> SolverImpl::MakeImplicationChecker(const Annotation& annotation) const {
	return std::make_unique<ImplicationCheckerImpl>(encoder, annotation);
}

struct Extractor : public LogicVisitor {
	const NowFormula* nowFormula = nullptr;
	const Annotation* annotation = nullptr;

	void visit(const AxiomConjunctionFormula& formula) override { nowFormula = &formula; }
	void visit(const ImplicationFormula& formula) override { nowFormula = &formula; }
	void visit(const ConjunctionFormula& formula) override { nowFormula = &formula; }
	void visit(const NegatedAxiom& formula) override { nowFormula = &formula; }
	void visit(const ExpressionAxiom& formula) override { nowFormula = &formula; }
	void visit(const OwnershipAxiom& formula) override { nowFormula = &formula; }
	void visit(const LogicallyContainedAxiom& formula) override { nowFormula = &formula; }
	void visit(const KeysetContainsAxiom& formula) override { nowFormula = &formula; }
	void visit(const HasFlowAxiom& formula) override { nowFormula = &formula; }
	void visit(const FlowContainsAxiom& formula) override { nowFormula = &formula; }
	void visit(const ObligationAxiom& formula) override { nowFormula = &formula; }
	void visit(const FulfillmentAxiom& formula) override { nowFormula = &formula; }
	void visit(const PastPredicate& /*formula*/) override { throw SolvingError("Cannot construct 'ImplicationChecker' for formula of type 'PastPredicate'."); }
	void visit(const FuturePredicate& /*formula*/) override { throw SolvingError("Cannot construct 'ImplicationChecker' for formula of type 'FuturePredicate'."); }
	void visit(const Annotation& formula) override { annotation = &formula; }
};

std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker(const Formula& formula) const {
	Extractor extractor;
	formula.accept(extractor);
	if (extractor.nowFormula) return std::make_unique<ImplicationCheckerImpl>(encoder, *extractor.nowFormula);
	assert(extractor.annotation);
	return std::make_unique<ImplicationCheckerImpl>(encoder, *extractor.annotation);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

inline std::unique_ptr<ConjunctionFormula> ComputeImplied(const SolverImpl& solver, const std::function<bool(const Formula&)>& IsImplied) {
	auto result = std::make_unique<ConjunctionFormula>();
	for (const auto& candidate : solver.GetCandidates()) {
		if (!IsImplied(candidate.GetCheck())) continue;
		assert(IsImplied(candidate.GetImplied())); // TODO: remove
		result = plankton::conjoin(std::move(result), plankton::copy(candidate.GetImplied()));
	}
	return result;
}

std::unique_ptr<ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const ImplicationCheckerImpl& checker) const {
	auto IsImplied = [&checker](const Formula& formula){ return checker.Implies(formula); };
	return ComputeImplied(*this, IsImplied);
}

std::unique_ptr<ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const std::vector<ImplicationCheckerImpl>& checkers) const {
	auto IsImplied = [&checkers](const Formula& formula){
		for (const auto& checker : checkers) {
			if (!checker.Implies(formula)) {
				return false;
			}
		}
		return true;
	};
	return ComputeImplied(*this, IsImplied);
}
