#include "plankton/verify.hpp"

#include <deque>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


const std::string AND_STR = "  ∧  ";
const std::string EMPTY_STR = "true";

template<typename T, typename U>
void print_elems(std::ostream& stream, const T& elems, U printer, std::string delim=AND_STR, std::string empty=EMPTY_STR) {
	if (elems.empty()) {
		stream << empty;
	} else {
		bool first = true;
		for (const auto& elem : elems) {
			if (first) {
				first = false;
			} else {
				stream << delim;
			}
			printer(elem);
		}
	}
}

struct FormulaPrinter : public LogicVisitor {
	std::ostream& stream;
	FormulaPrinter(std::ostream& stream_) : stream(stream_) {}

	void visit(const ConjunctionFormula& formula) override {
		print_elems(stream, formula.conjuncts, [this](const auto& e){ plankton::print(*e, this->stream); });
	}

	void visit(const ExpressionFormula& formula) override {
		cola::print(*formula.expr, stream);
	}

	void visit(const NegatedFormula& formula) override {
		stream << "¬[";
		plankton::print(*formula.formula, stream);
		stream << "]";
	}

	void visit(const OwnershipFormula& formula) override {
		stream << "@owned(";
		cola::print(*formula.expr, stream);
		stream << ")";
	}

	void visit(const LogicallyContainedFormula& formula) override {
		stream << "@contained(";
		cola::print(*formula.expr, stream);
		stream << ")";
	}

	void visit(const FlowFormula& formula) override {
		stream << "@flow(";
		cola::print(*formula.expr, stream);
		stream << ") = ";
		plankton::print(formula.flow, stream);
	}

	void visit(const ObligationFormula& /*formula*/) override {
		stream << "@OBL";
	}

	void visit(const FulfillmentFormula& /*formula*/) override {
		stream << "@FUL";
	}

	void visit(const PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: FormulaPrinter::visit(const PastPredicate&)"); }
	void visit(const FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: FormulaPrinter::visit(const FuturePredicate&)"); }
	void visit(const Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: FormulaPrinter::visit(const Annotation&)"); }
};

void plankton::print(const Formula& formula, std::ostream& stream) {
	FormulaPrinter printer(stream);
	formula.accept(printer);
}

void plankton::print(const PastPredicate& predicate, std::ostream& stream) {
	stream << "PAST<< ";
	plankton::print(*predicate.formula, stream);
	stream << " >>";
}

void plankton::print(const FuturePredicate& predicate, std::ostream& stream) {
	stream << "FUT<< ";
	plankton::print(*predicate.pre, stream);
	stream << " >> ";
	cola::print(*predicate.command, stream);
	stream << " << ";
	plankton::print(*predicate.post, stream);
	stream << " >>";
}

void plankton::print(const Annotation& annotation, std::ostream& stream) {
	stream << "{" << std::endl << "    " ;
	plankton::print(*annotation.now, stream);
	stream << std::endl << "  " + AND_STR << std::endl << "    ";
	print_elems(stream, annotation.past, [&stream](const auto& e){ plankton::print(e, stream); });
	stream << std::endl << "  " + AND_STR << std::endl << "    ";
	print_elems(stream, annotation.future, [&stream](const auto& e){ plankton::print(e, stream); });
	stream << std::endl << "}";
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

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
std::unique_ptr<ObligationFormula> plankton::copy<ObligationFormula>(const ObligationFormula& /*formula*/) {
	throw std::logic_error("not yet implemented: plankton::copy<ObligationFormula>(const ObligationFormula&)");
}

template<>
std::unique_ptr<FulfillmentFormula> plankton::copy<FulfillmentFormula>(const FulfillmentFormula& /*formula*/) {
	throw std::logic_error("not yet implemented: plankton::copy<FulfillmentFormula>(const FulfillmentFormula&)");
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
	void visit(const ObligationFormula& formula) override { result = plankton::copy(formula); }
	void visit(const FulfillmentFormula& formula) override { result = plankton::copy(formula); }
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
	void visit(const ObligationFormula& formula) override { result = plankton::copy(formula); }
	void visit(const FulfillmentFormula& formula) override { result = plankton::copy(formula); }
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
	auto assign = std::make_unique<Assignment>(cola::copy(*predicate.command->lhs), cola::copy(*predicate.command->rhs));
	return FuturePredicate(plankton::copy(*predicate.pre), std::move(assign), plankton::copy(*predicate.post));
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


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

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
