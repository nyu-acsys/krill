#include "plankton/verify.hpp"

#include <deque>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


inline std::unique_ptr<BasicFormula> copy_basic_formula(const BasicFormula& formula);

template<>
std::unique_ptr<BasicFormula> plankton::copy<BasicFormula>(const BasicFormula& formula) {
	return copy_basic_formula(formula);
}

template<>
std::unique_ptr<ExpressionFormula> plankton::copy<ExpressionFormula>(const ExpressionFormula& formula) {
	return std::make_unique<ExpressionFormula>(cola::copy(*formula.expr));
}

template<>
std::unique_ptr<NegatedFormula> plankton::copy<NegatedFormula>(const NegatedFormula& formula) {
	return std::make_unique<NegatedFormula>(plankton::copy(*formula.formula));
}

template<>
std::unique_ptr<OwnershipFormula> plankton::copy<OwnershipFormula>(const OwnershipFormula& formula) {
	return std::make_unique<OwnershipFormula>(std::make_unique<VariableExpression>(formula.expr->decl));
}

template<>
std::unique_ptr<LogicallyContainedFormula> plankton::copy<LogicallyContainedFormula>(const LogicallyContainedFormula& formula) {
	return std::make_unique<LogicallyContainedFormula>(cola::copy(*formula.expr));
}

template<>
std::unique_ptr<FlowFormula> plankton::copy<FlowFormula>(const FlowFormula& formula) {
	return std::make_unique<FlowFormula>(cola::copy(*formula.expr), formula.flow);
}

template<>
std::unique_ptr<ConjunctionFormula> plankton::copy<ConjunctionFormula>(const ConjunctionFormula& formula) {
	auto copy = std::make_unique<ConjunctionFormula>();
	for (const auto& conjunct : formula.conjuncts) {
		copy->conjuncts.push_back(plankton::copy(*conjunct));
	}
	return copy;
}

struct CopyBasicFormulaVisitor : public LogicVisitor {
	std::unique_ptr<BasicFormula> result;
	void visit(const ExpressionFormula& formula) override { result = plankton::copy(formula); }
	void visit(const NegatedFormula& formula) override { result = plankton::copy(formula); }
	void visit(const OwnershipFormula& formula) override { result = plankton::copy(formula); }
	void visit(const LogicallyContainedFormula& formula) override { result = plankton::copy(formula); }
	void visit(const FlowFormula& formula) override { result = plankton::copy(formula); }
	void visit(const ConjunctionFormula& /*formula*/) override { throw std::logic_error("Unexpected invocation: CopyBasicFormulaVisitor::visit(const ConjunctionFormula&)"); }
	void visit(const PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: CopyBasicFormulaVisitor::visit(const PastPredicate&)"); }
	void visit(const FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: CopyBasicFormulaVisitor::visit(const FuturePredicate&)"); }
	void visit(const Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: CopyBasicFormulaVisitor::visit(const Annotation&)"); }
};

inline std::unique_ptr<BasicFormula> copy_basic_formula(const BasicFormula& formula) {
	CopyBasicFormulaVisitor visitor;
	formula.accept(visitor);
	return std::move(visitor.result);
}

struct CopyFormulaVisitor : public LogicVisitor {
	std::unique_ptr<Formula> result;
	void visit(const ConjunctionFormula& formula) override { result = plankton::copy(formula); }
	void visit(const ExpressionFormula& formula) override { result = plankton::copy(formula); }
	void visit(const NegatedFormula& formula) override { result = plankton::copy(formula); }
	void visit(const OwnershipFormula& formula) override { result = plankton::copy(formula); }
	void visit(const LogicallyContainedFormula& formula) override { result = plankton::copy(formula); }
	void visit(const FlowFormula& formula) override { result = plankton::copy(formula); }
	void visit(const PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: CopyFormulaVisitor::visit(const PastPredicate&)"); }
	void visit(const FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: CopyFormulaVisitor::visit(const FuturePredicate&)"); }
	void visit(const Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: CopyFormulaVisitor::visit(const Annotation&)"); }
};

template<>
std::unique_ptr<Formula> plankton::copy<Formula>(const Formula& formula) {
	CopyFormulaVisitor visitor;
	formula.accept(visitor);
	return std::move(visitor.result);
}

PastPredicate plankton::copy(const PastPredicate& predicate) {
	return PastPredicate(plankton::copy(*predicate.formula));
}

FuturePredicate plankton::copy(const FuturePredicate& predicate) {
	return FuturePredicate(
		plankton::copy(*predicate.pre), cola::copy(*predicate.command), plankton::copy(*predicate.post)
	);
}

std::unique_ptr<Annotation> plankton::copy(const Annotation& annotation) {
	auto now = std::make_unique<ConjunctionFormula>();
	for (const auto& conjunct : annotation.now->conjuncts) {
		now->conjuncts.push_back(plankton::copy(*conjunct));
	}
	auto result = std::make_unique<Annotation>(std::move(now));
	for (const auto& past : annotation.past) {
		result->past.push_back(plankton::copy(past));
	}
	for (const auto& fut : annotation.future) {
		result->future.push_back(plankton::copy(fut));
	}
	return result;
}


inline std::unique_ptr<Annotation> mk_bool(bool value) {
	auto now = std::make_unique<ConjunctionFormula>();
	now->conjuncts.push_back(std::make_unique<ExpressionFormula>(std::make_unique<BooleanValue>(value)));
	return std::make_unique<Annotation>(std::move(now));
}

std::unique_ptr<Annotation> plankton::make_true() {
	return mk_bool(true);
}

std::unique_ptr<Annotation> plankton::make_false() {
	return mk_bool(false);
}



bool plankton::implies_nonnull(const Formula& formula, const cola::Expression& expr) {
	// TODO: avoid copying expr (super ugly solution: create a unique_ptr from its raw pointer and release the unique_ptr afterwards)
	auto nonnull = std::make_unique<BinaryExpression>(BinaryExpression::Operator::NEQ, cola::copy(expr), std::make_unique<NullValue>());
	return plankton::implies(formula, *nonnull);
}

bool plankton::implies(const Formula& /*formula*/, const Expression& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::implies(const Formula& /*formula*/, const Formula& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::is_equal(const Annotation& /*annotation*/, const Annotation& /*other*/) {
	throw std::logic_error("not yet implemented (plankton::is_equal)");
}

std::unique_ptr<Annotation> plankton::unify(const Annotation& /*annotation*/, const Annotation& /*other*/) {
	// creates an annotation that implies the passed annotations
	throw std::logic_error("not yet implemented (plankton::unify)");
}

std::unique_ptr<Annotation> plankton::unify(const std::vector<std::unique_ptr<Annotation>>& /*annotations*/) {
	// creates an annotation that implies the passed annotations
	throw std::logic_error("not yet implemented (plankton::unify)");
} 
