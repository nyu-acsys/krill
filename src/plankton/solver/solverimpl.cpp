#include "plankton/solver/solverimpl.hpp"

#include "plankton/error.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


Candidate::Candidate(std::unique_ptr<SimpleFormula> candidate) : store(std::make_unique<ConjunctionFormula>()) {
	store->conjuncts.push_back(std::move(candidate));
}

Candidate::Candidate(std::unique_ptr<SimpleFormula> candidate, std::unique_ptr<ConjunctionFormula> implied) : Candidate(std::move(candidate)) {
	store = plankton::conjoin(std::move(store), std::move(implied));
}

Candidate::Candidate(std::unique_ptr<ConjunctionFormula> store_) : store(std::move(store_)) {}

const SimpleFormula& Candidate::GetCheck() const {
	return *store->conjuncts.at(0);
}

const ConjunctionFormula& Candidate::GetImplied() const {
	return *store;
}

Candidate Candidate::Copy() const {
	return Candidate(plankton::copy(*store));
}


SolverImpl::SolverImpl(PostConfig config_) : Solver(std::move(config_)), encoder(config) {
}

std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker(const NowFormula& formula) const {
	return std::make_unique<ImplicationCheckerImpl>(encoder, formula);
}

std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker(const Annotation& annotation) const {
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


inline std::unique_ptr<ConjunctionFormula> ComputeImplied(const SolverImpl& solver, std::function<bool(const Candidate&)> isImplied) {
	auto result = std::make_unique<ConjunctionFormula>();
	for (const auto& candidate : solver.GetCandidates()) {
		if (isImplied(candidate)) {
			result = plankton::conjoin(std::move(result), plankton::copy(candidate.GetImplied()));
		}
	}
	return result;
}

std::unique_ptr<ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const ImplicationChecker& checker) const {
	return ComputeImplied(*this, [&checker](const Candidate& candidate){
		return checker.Implies(candidate.GetCheck());
	});
}

std::unique_ptr<ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const std::vector<ImplicationCheckerImpl>& checkers) const {
	return ComputeImplied(*this, [&checkers](const Candidate& candidate){
		for (const auto& checker : checkers) {
			if (!checker.Implies(candidate.GetCheck())) {
				return false;
			}
		}
		return true;
	});
}
