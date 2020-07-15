#include "plankton/solver/solverimpl.hpp"

#include "plankton/error.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


Candidate::Candidate(std::unique_ptr<SimpleFormula> candidate)
	 : store(std::make_unique<ConjunctionFormula>()), disprove(std::make_unique<ConjunctionFormula>())
{
	assert(candidate);
	store->conjuncts.push_back(std::move(candidate));
}

Candidate::Candidate(std::unique_ptr<SimpleFormula> candidate, std::unique_ptr<ConjunctionFormula> implied) : Candidate(std::move(candidate)) {
	assert(implied);
	store = plankton::conjoin(std::move(store), std::move(implied));
}

Candidate::Candidate(std::unique_ptr<SimpleFormula> candidate, std::unique_ptr<ConjunctionFormula> implied, std::deque<std::unique_ptr<SimpleFormula>> disprove_)
	: Candidate(std::move(candidate), std::move(implied))
{
	disprove = std::make_unique<ConjunctionFormula>();
	disprove->conjuncts = std::move(disprove_);
}

Candidate::Candidate(std::unique_ptr<ConjunctionFormula> store_, std::unique_ptr<ConjunctionFormula> disprove_)
	: store(std::move(store_)), disprove(std::move(disprove_))
{
	assert(store);
	assert(disprove);
}

const SimpleFormula& Candidate::GetCheck() const {
	return *store->conjuncts.at(0);
}

const ConjunctionFormula& Candidate::GetImplied() const {
	return *store;
}

const std::deque<std::unique_ptr<SimpleFormula>>& Candidate::GetQuickDisprove() const {
	return disprove->conjuncts;
}

Candidate Candidate::Copy() const {
	return Candidate(plankton::copy(*store), plankton::copy(*disprove));
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

struct ImplicationComputer {
	const SolverImpl& solver;
	std::function<bool(const SimpleFormula&)> IsImplied, IsImpliedQuick;
	std::unique_ptr<ConjunctionFormula> result;

	ImplicationComputer(const SolverImpl& solver, std::function<bool(const SimpleFormula&)> implication,
		std::function<bool(const SimpleFormula&)> quick)	: solver(solver), IsImplied(implication), IsImpliedQuick(quick) {}

	inline bool QuickCheck(const SimpleFormula& formula) {
		return plankton::syntactically_contains_conjunct(*result, formula) || IsImpliedQuick(formula);
	}

	inline bool QuickCheck(const Candidate& candidate) {
		return QuickCheck(candidate.GetCheck());
	}

	inline bool QuickDisprove(const Candidate& candidate) {
		for (const auto& formula : candidate.GetQuickDisprove()) {
			if (QuickCheck(*formula)) return true;
		}
		return false;
	}

	std::unique_ptr<ConjunctionFormula> ComputeImplied(){
		result = std::make_unique<ConjunctionFormula>();
		for (const auto& candidate : solver.GetCandidates()) {
			// quick check: try to prove candidate
			bool takeCandidate = QuickCheck(candidate.GetCheck());

			// try to syntactically disprove the candidate
			if (!takeCandidate && QuickDisprove(candidate)) continue;

			// do a semantic implication check
			if (!takeCandidate && !IsImplied(candidate.GetCheck())) continue;

			// add candidate
			result = plankton::conjoin(std::move(result), plankton::copy(candidate.GetImplied()));
		}
		return std::move(result);
	}
};

std::unique_ptr<ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const ImplicationCheckerImpl& checker) const {
	auto IsImplied = [&checker](const SimpleFormula& formula){ return checker.Implies(formula); };
	auto IsImpliedQuick = [&checker](const SimpleFormula& formula){ return checker.ImpliesQuick(formula); };
	return ImplicationComputer(*this, IsImplied, IsImpliedQuick).ComputeImplied();
}

std::unique_ptr<ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const std::vector<ImplicationCheckerImpl>& checkers) const {
	auto IsImplied = [&checkers](const SimpleFormula& formula){
		for (const auto& checker : checkers) {
			if (!checker.Implies(formula)) {
				return false;
			}
		}
		return true;
	};
	auto IsImpliedQuick = [&checkers](const SimpleFormula& formula){
		for (const auto& checker : checkers) {
			if (!checker.ImpliesQuick(formula)) {
				return false;
			}
		}
		return true;
	};
	return ImplicationComputer(*this, IsImplied, IsImpliedQuick).ComputeImplied();
}
