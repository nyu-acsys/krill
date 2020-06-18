#include "plankton/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


template<> std::unique_ptr<Axiom> plankton::copy<Axiom>(const Axiom& formula);
template<> std::unique_ptr<SimpleFormula> plankton::copy<SimpleFormula>(const SimpleFormula& formula);
template<> std::unique_ptr<TimePredicate> plankton::copy<TimePredicate>(const TimePredicate& formula);

template<>
std::unique_ptr<AxiomConjunctionFormula> plankton::copy<AxiomConjunctionFormula>(const AxiomConjunctionFormula& formula) {
	auto copy = std::make_unique<AxiomConjunctionFormula>();
	for (const auto& conjunct : formula.conjuncts) {
		copy->conjuncts.push_back(plankton::copy(*conjunct));
	}
	return copy;
}

template<>
std::unique_ptr<ImplicationFormula> plankton::copy<ImplicationFormula>(const ImplicationFormula& formula) {
	return std::make_unique<ImplicationFormula>(plankton::copy(*formula.premise), plankton::copy(*formula.conclusion));
}

template<>
std::unique_ptr<ConjunctionFormula> plankton::copy<ConjunctionFormula>(const ConjunctionFormula& formula) {
	auto copy = std::make_unique<ConjunctionFormula>();
	for (const auto& axiom : formula.axioms) {
		copy->axioms.push_back(plankton::copy(*axiom));
	}
	for (const auto& implication : formula.implications) {
		copy->implications.push_back(plankton::copy(*implication));
	}
	return copy;
}

template<>
std::unique_ptr<NegatedAxiom> plankton::copy<NegatedAxiom>(const NegatedAxiom& formula) {
	return std::make_unique<NegatedAxiom>(plankton::copy(*formula.axiom));
}

template<>
std::unique_ptr<ExpressionAxiom> plankton::copy<ExpressionAxiom>(const ExpressionAxiom& formula) {
	return std::make_unique<ExpressionAxiom>(cola::copy(*formula.expr));
}

template<>
std::unique_ptr<OwnershipAxiom> plankton::copy<OwnershipAxiom>(const OwnershipAxiom& formula) {
	return std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(formula.expr->decl));
}

template<>
std::unique_ptr<LogicallyContainedAxiom> plankton::copy<LogicallyContainedAxiom>(const LogicallyContainedAxiom& formula) {
	return std::make_unique<LogicallyContainedAxiom>(cola::copy(*formula.expr));
}

template<>
std::unique_ptr<KeysetContainsAxiom> plankton::copy<KeysetContainsAxiom>(const KeysetContainsAxiom& formula) {
	return std::make_unique<KeysetContainsAxiom>(cola::copy(*formula.node), cola::copy(*formula.value));
}

template<>
std::unique_ptr<FlowAxiom> plankton::copy<FlowAxiom>(const FlowAxiom& formula) {
	return std::make_unique<FlowAxiom>(cola::copy(*formula.expr), formula.flow);
}

template<>
std::unique_ptr<ObligationAxiom> plankton::copy<ObligationAxiom>(const ObligationAxiom& formula) {
	return std::make_unique<ObligationAxiom>(formula.kind, std::make_unique<VariableExpression>(formula.key->decl));
}

template<>
std::unique_ptr<FulfillmentAxiom> plankton::copy<FulfillmentAxiom>(const FulfillmentAxiom& formula) {
	return std::make_unique<FulfillmentAxiom>(formula.kind, std::make_unique<VariableExpression>(formula.key->decl), formula.return_value);
}

template<>
std::unique_ptr<PastPredicate> plankton::copy<PastPredicate>(const PastPredicate& formula) {
	return std::make_unique<PastPredicate>(plankton::copy(*formula.formula));
}

template<>
std::unique_ptr<FuturePredicate> plankton::copy<FuturePredicate>(const FuturePredicate& formula) {
	return std::make_unique<FuturePredicate>(
		plankton::copy(*formula.pre),
		std::make_unique<Assignment>(cola::copy(*formula.command->lhs), cola::copy(*formula.command->rhs)),
		plankton::copy(*formula.post)
	);
}

template<>
std::unique_ptr<Annotation> plankton::copy<Annotation>(const Annotation& formula) {
	auto copy = std::make_unique<Annotation>(plankton::copy(*formula.now));
	for (const auto& pred : formula.time) {
		copy->time.push_back(plankton::copy(*pred));
	}
	return copy;
}


template<typename T>
struct CopyVisitor : public LogicVisitor {
	std::unique_ptr<T> result;

	void visit(const AxiomConjunctionFormula& formula) { result = plankton::copy<AxiomConjunctionFormula>(formula); }
	void visit(const ImplicationFormula& formula) { result = plankton::copy<ImplicationFormula>(formula); }
	void visit(const ConjunctionFormula& formula) { result = plankton::copy<ConjunctionFormula>(formula); }
	void visit(const NegatedAxiom& formula) { result = plankton::copy<NegatedAxiom>(formula); }
	void visit(const ExpressionAxiom& formula) { result = plankton::copy<ExpressionAxiom>(formula); }
	void visit(const OwnershipAxiom& formula) { result = plankton::copy<OwnershipAxiom>(formula); }
	void visit(const LogicallyContainedAxiom& formula) { result = plankton::copy<LogicallyContainedAxiom>(formula); }
	void visit(const KeysetContainsAxiom& formula) { result = plankton::copy<KeysetContainsAxiom>(formula); }
	void visit(const FlowAxiom& formula) { result = plankton::copy<FlowAxiom>(formula); }
	void visit(const ObligationAxiom& formula) { result = plankton::copy<ObligationAxiom>(formula); }
	void visit(const FulfillmentAxiom& formula) { result = plankton::copy<FulfillmentAxiom>(formula); }
	void visit(const PastPredicate& formula) { result = plankton::copy<PastPredicate>(formula); }
	void visit(const FuturePredicate& formula) { result = plankton::copy<FuturePredicate>(formula); }
	void visit(const Annotation& formula) { result = plankton::copy<Annotation>(formula); }
};

template<typename F, typename X>
std::unique_ptr<F> plankton::copy(const F& formula) {
	CopyVisitor<F> visitor;
	formula.accept(visitor);
	return std::move(visitor.result);
}
