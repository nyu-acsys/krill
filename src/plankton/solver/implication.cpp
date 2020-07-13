#include "plankton/solver/solverimpl.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "plankton/config.hpp"
#include "plankton/error.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


struct SimpleFormulaContainer final : public ConjunctContainer {
	std::array<const SimpleFormula*, 1> container;
	SimpleFormulaContainer(const SimpleFormula* formula) : container({ formula }) {}
	std::iterator<std::forward_iterator_tag, const SimpleFormula> begin() override { return container.begin(); }
	std::iterator<std::forward_iterator_tag, const SimpleFormula> end() override { return container.end(); }
};

template<typename T>
struct ConjunctionContainer final : public ConjunctContainer {
	const T& conjunction;
	ConjunctContainer(const T& conjunction) : conjunction(conjunction) {}
	std::iterator<std::forward_iterator_tag, const SimpleFormula> begin() override { return conjunction.conjuncts.begin(); }
	std::iterator<std::forward_iterator_tag, const SimpleFormula> end() override { return conjunction.conjuncts.end(); }
}

std::unique_ptr<ConjunctContainer> MakeIterable(const SimpleFormula& formula) {
	return std::make_unique<SimpleFormulaContainer>(formula);
}

std::unique_ptr<ConjunctContainer> MakeIterable(const ConjunctionFormula& formula) {
	return std::make_unique<ConjunctionContainer>(formula);
}

std::unique_ptr<ConjunctContainer> MakeIterable(const AxiomConjunctionFormula& formula) {
	return std::make_unique<ConjunctionContainer>(formula);
}





ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, z3::solver solver_, std::pair<iterator_t, iterator_t> premise_, Encoder::StepTag tag_)
	: encoder(encoder_), solver(solver_), premiseConjunctsBegin(premise_.first), premiseConjunctsEnd(premise_.second), encodingTag(tag_)
{
}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, z3::solver solver_, const Formula& premise_, Encoder::StepTag tag_)
	: ImplicationCheckerImpl(encoder_, solver_, MakePremiseIterators(premise_), tag_)
{
	solver.add(encoder.Encode(premise_, encodingTag));
}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, const Formula& premise_, Encoder::StepTag tag_)
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
			// TODO important: solver.add(expr) ?
			return true;
	}
}

bool ImplicationCheckerImpl::Implies(z3::expr expr) const {
	return Implies(solver, expr);
}

// bool ImplicationCheckerImpl::ImpliesFalse() const {
// 	return Implies(encoder.MakeFalse());
// }

// bool ImplicationCheckerImpl::Implies(const Formula& implied) const {
// 	return Implies(encoder.Encode(implied, encodingTag));
// }

// bool ImplicationCheckerImpl::Implies(const Expression& implied) const {
// 	if (implied.sort() != Sort::BOOL) {
// 		throw SolvingError("Cannot check implication for expression: expression is not of boolean sort.");
// 	}
// 	return Implies(encoder.Encode(implied, encodingTag));
// }

// bool ImplicationCheckerImpl::ImpliesNonNull(const Expression& nonnull) const {
// 	return Implies(encoder.Encode(nonnull, encodingTag) != encoder.MakeNullPtr());
// }



// struct QuickCheckEncoder : public LogicVisitor {
// 	Encoder& encoder;
// 	const ConjunctionFormula& premise;
// 	z3::expr_vector conjuncts;

// 	QuickCheckEncoder(Encoder& encoder, const ConjunctionFormula& premise) : encoder(encoder), premise(premise), conjuncts(encoder.MakeVector()) {}

// 	void handle(const SimpleFormula& formula) {
// 		if (!plankton::syntactically_contains_conjunct(premise, formula)) {
// 			conjuncts.push_back(encoder.Encode(formula));
// 		}
// 	}

// 	template<typename T>
// 	void handle(const T& formula) {
// 		for (const auto& conjunct : formula.conjuncts) {
// 			handle(*conjunct);
// 		}
// 	}

// 	void visit(const AxiomConjunctionFormula& formula) override { handle(formula); }
// 	void visit(const ImplicationFormula& formula) override { handle(formula); }
// 	void visit(const ConjunctionFormula& formula) override { handle(formula); }
// 	void visit(const NegatedAxiom& formula) override { handle(formula); }
// 	void visit(const ExpressionAxiom& formula) override { handle(formula); }
// 	void visit(const OwnershipAxiom& formula) override { handle(formula); }
// 	void visit(const LogicallyContainedAxiom& formula) override { handle(formula); }
// 	void visit(const KeysetContainsAxiom& formula) override { handle(formula); }
// 	void visit(const HasFlowAxiom& formula) override { handle(formula); }
// 	void visit(const FlowContainsAxiom& formula) override { handle(formula); }
// 	void visit(const ObligationAxiom& formula) override { handle(formula); }
// 	void visit(const FulfillmentAxiom& formula) override { handle(formula); }

// 	void visit(const PastPredicate& /*formula*/) override { throw SolvingError("Cannot check implication for 'PastPredicate'."); }
// 	void visit(const FuturePredicate& /*formula*/) override { throw SolvingError("Cannot check implication for 'FuturePredicate'."); }
// 	void visit(const Annotation& /*formula*/) override { throw SolvingError("Cannot check implication for 'Annotation'."); }
// };

bool ImplicationCheckerImpl::Implies(const Formula& implied) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::Implies(Formula)");
	// QuickCheckEncoder checker(encoder, premise);
	// implied.accept(checker);
	// if (checker.conjuncts.size() == 0) return true;
	// return Implies(encoder.MakeAnd(std::move(checker.conjuncts)));
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
