#include "plankton/solver/solverimpl.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "plankton/config.hpp"
#include "plankton/error.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, z3::solver solver_, const NowFormula& premise_, Encoder::StepTag tag_)
	: encoder(encoder_), solver(solver_), encodingTag(tag_), premiseNowFormula(&premise_), premiseAnnotation(nullptr) {
	solver.add(encoder.Encode(*premiseNowFormula, encodingTag));
}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, z3::solver solver_, const Annotation& premise_, Encoder::StepTag tag_)
	: encoder(encoder_), solver(solver_), encodingTag(tag_), premiseNowFormula(premise_.now.get()), premiseAnnotation(&premise_) {
	assert(premiseNowFormula);
	solver.add(encoder.Encode(*premiseNowFormula, encodingTag));
}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, const NowFormula& premise_, Encoder::StepTag tag_)
	: ImplicationCheckerImpl(encoder_, encoder_.MakeSolver(), premise_, tag_) {}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, const Annotation& premise_, Encoder::StepTag tag_)
	: ImplicationCheckerImpl(encoder_, encoder_.MakeSolver(), premise_, tag_) {}


bool ImplicationCheckerImpl::Implies(z3::solver& solver, z3::expr expr) {
	// add negated 'expr' and check satisfiability
	solver.push();
	solver.add(!expr);
	auto result = solver.check();
	solver.pop();

	// analyse result
	switch (result) {
		case z3::unknown:
			if (plankton::config.z3_handle_unknown_result) return false;
			throw SolvingError("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");
		case z3::sat:
			return false;
		case z3::unsat:
			return true;
	}
}

bool ImplicationCheckerImpl::Implies(z3::expr expr) const {
	return Implies(solver, expr);
}


bool ImplicationCheckerImpl::Implies(const Expression& implied) const {
	if (implied.sort() != Sort::BOOL) {
		throw SolvingError("Cannot check implication for expression: expression is not of boolean sort.");
	}
	ExpressionAxiom axiom(cola::copy(implied));
	return Implies(axiom);
}

bool ImplicationCheckerImpl::ImpliesNonNull(const Expression& nonnull) const {
	BinaryExpression nonNull(BinaryExpression::Operator::NEQ, cola::copy(nonnull), std::make_unique<NullValue>());
	return Implies(nonNull);
}

bool ImplicationCheckerImpl::ImpliesFalse() const {
	BooleanValue value(false);
	return Implies(value);
}


struct Dispatcher : public LogicVisitor {
	const ImplicationCheckerImpl& checker;
	bool result;

	Dispatcher(const ImplicationCheckerImpl& checker) : checker(checker) {}

	bool Implies(const Formula& formula) {
		formula.accept(*this);
		return result;
	}

	void visit(const AxiomConjunctionFormula& formula) override { result = checker.Implies(formula); }
	void visit(const ImplicationFormula& formula) override { result = checker.Implies(formula); }
	void visit(const ConjunctionFormula& formula) override { result = checker.Implies(formula); }
	void visit(const NegatedAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const ExpressionAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const OwnershipAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const LogicallyContainedAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const KeysetContainsAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const HasFlowAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const FlowContainsAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const ObligationAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const FulfillmentAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const PastPredicate& formula) override { result = checker.Implies(formula); }
	void visit(const FuturePredicate& formula) override { result = checker.Implies(formula); }
	void visit(const Annotation& formula) override { result = checker.Implies(formula); }
};

bool ImplicationCheckerImpl::Implies(const Formula& implied) const {
	return Dispatcher(*this).Implies(implied);
}


struct QuickChecker : public BaseLogicVisitor {
	Encoder& encoder;
	Encoder::StepTag encodingTag;
	const NowFormula& premise;
	z3::expr_vector conjuncts;

	QuickChecker(Encoder& encoder, Encoder::StepTag tag, const NowFormula& premise)
		: encoder(encoder), encodingTag(tag), premise(premise), conjuncts(encoder.MakeVector()) {}

	z3::expr_vector EncodeRemaining(const NowFormula& implied) {
		implied.accept(*this);
		return std::move(conjuncts);
	}

	void handle(const SimpleFormula& formula) {
		if (!plankton::syntactically_contains_conjunct(premise, formula)) {
			conjuncts.push_back(encoder.Encode(formula, encodingTag));
		}
	}

	template<typename T>
	void handle_conjunction(const T& formula) {
		for (const auto& conjunct : formula.conjuncts) {
			handle(*conjunct);
		}
	}

	void visit(const ConjunctionFormula& formula) override { handle_conjunction(formula); }
	void visit(const AxiomConjunctionFormula& formula) override { handle_conjunction(formula); }

	void visit(const ImplicationFormula& formula) override { handle(formula); }
	void visit(const NegatedAxiom& formula) override { handle(formula); }
	void visit(const ExpressionAxiom& formula) override { handle(formula); }
	void visit(const OwnershipAxiom& formula) override { handle(formula); }
	void visit(const LogicallyContainedAxiom& formula) override { handle(formula); }
	void visit(const KeysetContainsAxiom& formula) override { handle(formula); }
	void visit(const HasFlowAxiom& formula) override { handle(formula); }
	void visit(const FlowContainsAxiom& formula) override { handle(formula); }
	void visit(const ObligationAxiom& formula) override { handle(formula); }
	void visit(const FulfillmentAxiom& formula) override { handle(formula); }
};


bool ImplicationCheckerImpl::Implies(const NowFormula& implied) const {
	assert(premiseNowFormula);
	auto conjuncts = QuickChecker(encoder, encodingTag, *premiseNowFormula).EncodeRemaining(implied);
	if (conjuncts.size() == 0) return true;
	// TODO: if the implication holds, one could extend 'this->premiseNow' with 'implied'
	return Implies(encoder.MakeAnd(std::move(conjuncts)));
}

bool ImplicationCheckerImpl::Implies(const PastPredicate& /*implied*/) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::Implies(const PastPredicate&)");
}

bool ImplicationCheckerImpl::Implies(const FuturePredicate& /*implied*/) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::Implies(const FuturePredicate&)");
}

bool ImplicationCheckerImpl::Implies(const TimePredicate& implied) const {
	return Dispatcher(*this).Implies(implied);
}

bool ImplicationCheckerImpl::Implies(const Annotation& implied) const {
	if (!Implies(*implied.now)) return false;
	for (const auto& predicate : implied.time) {
		if (!Implies(*predicate)) return false;
	}
	return true;
}

