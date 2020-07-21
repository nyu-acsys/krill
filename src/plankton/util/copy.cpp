#include "plankton/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


std::unique_ptr<AxiomConjunctionFormula> plankton::copy(const AxiomConjunctionFormula& formula) {
	auto copy = std::make_unique<AxiomConjunctionFormula>();
	for (const auto& conjunct : formula.conjuncts) {
		copy->conjuncts.push_back(plankton::copy(*conjunct));
	}
	return copy;
}

std::unique_ptr<ImplicationFormula> plankton::copy(const ImplicationFormula& formula) {
	return std::make_unique<ImplicationFormula>(plankton::copy(*formula.premise), plankton::copy(*formula.conclusion));
}

std::unique_ptr<ConjunctionFormula> plankton::copy(const ConjunctionFormula& formula) {
	auto copy = std::make_unique<ConjunctionFormula>();
	for (const auto& conjunct : formula.conjuncts) {
		copy->conjuncts.push_back(plankton::copy(*conjunct));
	}
	return copy;
}

std::unique_ptr<NegatedAxiom> plankton::copy(const NegatedAxiom& formula) {
	return std::make_unique<NegatedAxiom>(plankton::copy(*formula.axiom));
}

std::unique_ptr<ExpressionAxiom> plankton::copy(const ExpressionAxiom& formula) {
	return std::make_unique<ExpressionAxiom>(cola::copy(*formula.expr));
}

std::unique_ptr<OwnershipAxiom> plankton::copy(const OwnershipAxiom& formula) {
	return std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(formula.expr->decl));
}

std::unique_ptr<NodeLogicallyContainsAxiom> plankton::copy(const NodeLogicallyContainsAxiom& formula) {
	return std::make_unique<NodeLogicallyContainsAxiom>(cola::copy(*formula.node), cola::copy(*formula.value));
}

std::unique_ptr<DataStructureLogicallyContainsAxiom> plankton::copy(const DataStructureLogicallyContainsAxiom& formula) {
	return std::make_unique<DataStructureLogicallyContainsAxiom>(cola::copy(*formula.value));
}

std::unique_ptr<KeysetContainsAxiom> plankton::copy(const KeysetContainsAxiom& formula) {
	return std::make_unique<KeysetContainsAxiom>(cola::copy(*formula.node), cola::copy(*formula.value));
}

std::unique_ptr<HasFlowAxiom> plankton::copy(const HasFlowAxiom& formula) {
	return std::make_unique<HasFlowAxiom>(cola::copy(*formula.expr));
}

std::unique_ptr<FlowContainsAxiom> plankton::copy(const FlowContainsAxiom& formula) {
	return std::make_unique<FlowContainsAxiom>(cola::copy(*formula.expr), cola::copy(*formula.low_value), cola::copy(*formula.high_value));
}

std::unique_ptr<ObligationAxiom> plankton::copy(const ObligationAxiom& formula) {
	return std::make_unique<ObligationAxiom>(formula.kind, std::make_unique<VariableExpression>(formula.key->decl));
}

std::unique_ptr<FulfillmentAxiom> plankton::copy(const FulfillmentAxiom& formula) {
	return std::make_unique<FulfillmentAxiom>(formula.kind, std::make_unique<VariableExpression>(formula.key->decl), formula.return_value);
}

std::unique_ptr<PastPredicate> plankton::copy(const PastPredicate& formula) {
	return std::make_unique<PastPredicate>(plankton::copy(*formula.formula));
}

std::unique_ptr<FuturePredicate> plankton::copy(const FuturePredicate& formula) {
	return std::make_unique<FuturePredicate>(
		plankton::copy(*formula.pre),
		std::make_unique<Assignment>(cola::copy(*formula.command->lhs), cola::copy(*formula.command->rhs)),
		plankton::copy(*formula.post)
	);
}

std::unique_ptr<Annotation> plankton::copy(const Annotation& formula) {
	auto copy = std::make_unique<Annotation>(plankton::copy(*formula.now));
	for (const auto& pred : formula.time) {
		copy->time.push_back(plankton::copy(*pred));
	}
	return copy;
}


template<typename T>
struct CopyVisitor : public LogicVisitor {
	std::unique_ptr<T> result;

	template<typename U>
	std::enable_if_t<std::is_base_of_v<T, U>> set_result(std::unique_ptr<U> uptr) {
		result = std::move(uptr);
	}

	template<typename U>
	std::enable_if_t<!std::is_base_of_v<T, U>> set_result(std::unique_ptr<U> /*uptr*/) {
		throw std::logic_error("Wargelgarbel: could not copy formula... sorry.");
	}

	void visit(const ConjunctionFormula& formula) { set_result(plankton::copy(formula)); }
	void visit(const Annotation& formula) { set_result(plankton::copy(formula)); }
	void visit(const AxiomConjunctionFormula& formula) { set_result(plankton::copy(formula)); }

	void visit(const ImplicationFormula& formula) { set_result(plankton::copy(formula)); }

	void visit(const NegatedAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const ExpressionAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const OwnershipAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const DataStructureLogicallyContainsAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const NodeLogicallyContainsAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const KeysetContainsAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const HasFlowAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const FlowContainsAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const ObligationAxiom& formula) { set_result(plankton::copy(formula)); }
	void visit(const FulfillmentAxiom& formula) { set_result(plankton::copy(formula)); }

	void visit(const PastPredicate& formula) { set_result(plankton::copy(formula)); }
	void visit(const FuturePredicate& formula) { set_result(plankton::copy(formula)); }
};

template<typename T>
std::unique_ptr<T> do_copy(const T& formula) {
	CopyVisitor<T> visitor;
	formula.accept(visitor);
	return std::move(visitor.result);
}

std::unique_ptr<Formula> plankton::copy(const Formula& formula) {
	return do_copy<Formula>(formula);
}

std::unique_ptr<NowFormula> plankton::copy(const NowFormula& formula) {
	return do_copy<NowFormula>(formula);
}

std::unique_ptr<SimpleFormula> plankton::copy(const SimpleFormula& formula) {
	return do_copy<SimpleFormula>(formula);
}

std::unique_ptr<Axiom> plankton::copy(const Axiom& formula) {
	return do_copy<Axiom>(formula);
}

std::unique_ptr<TimePredicate> plankton::copy(const TimePredicate& formula) {
	return do_copy<TimePredicate>(formula);
}
