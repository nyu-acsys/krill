#include "plankton/util.hpp"

#include <sstream>
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

	template<typename T>
	static void print(const T& obj, std::ostream& stream) {
		FormulaPrinter printer(stream);
		obj.accept(printer);
	}

	void visit(const ConjunctionFormula& formula) override {
		print_elems(stream, formula.conjuncts, [this](const auto& e){
			e->accept(*this);
		});
	}

	void visit(const ImplicationFormula& formula) override {
		stream << "[";
		formula.premise->accept(*this);
		stream << "]==>";
		stream << "[";
		formula.conclusion->accept(*this);
		stream << "]";
	}

	void visit(const ExpressionFormula& formula) override {
		cola::print(*formula.expr, stream);
	}

	void visit(const NegatedFormula& formula) override {
		stream << "¬[";
		formula.formula->accept(*this);
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

	void visit(const PastPredicate& predicate) override {
		stream << "PAST<< ";
		predicate.formula->accept(*this);
		stream << " >>";
	}
	
	void visit(const FuturePredicate& predicate) override {
		stream << "FUT<< ";
		predicate.pre->accept(*this);
		stream << " >> ";
		cola::print(*predicate.command, stream);
		stream << " << ";
		predicate.post->accept(*this);
		stream << " >>";
	}

	void visit(const Annotation& annotation) override {
		stream << "{" << std::endl << "    " ;
		annotation.now->accept(*this);
		stream << std::endl << "  " + AND_STR << std::endl << "    ";
		print_elems(stream, annotation.time, [this](const auto& e){ e->accept(*this); });
		stream << std::endl << "}";
	}

};

void plankton::print(const Formula& formula, std::ostream& stream) {
	FormulaPrinter::print(formula, stream);
}

void plankton::print(const TimePredicate& predicate, std::ostream& stream) {
	FormulaPrinter::print(predicate, stream);
}

void plankton::print(const Annotation& annotation, std::ostream& stream) {
	FormulaPrinter::print(annotation, stream);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct CopyVisitor : public LogicVisitor {
	std::unique_ptr<Formula> result_formula;
	std::unique_ptr<TimePredicate> result_predicate;
	std::unique_ptr<Annotation> result_annotation;

	template<typename T>
	CopyVisitor(const T& obj) {
		obj.accept(*this);
	}

	void visit(const ConjunctionFormula& formula) override {
		auto copy = std::make_unique<ConjunctionFormula>();
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
			assert(result_formula);
			copy->conjuncts.push_back(std::move(result_formula));
		}
		result_formula = std::move(copy);
	}

	void visit(const ImplicationFormula& formula) override {
		formula.premise->accept(*this);
		auto premise = std::move(result_formula);
		formula.conclusion->accept(*this);
		result_formula = std::make_unique<ImplicationFormula>(std::move(premise), std::move(result_formula));
	}

	void visit(const ExpressionFormula& formula) override {
		result_formula = std::make_unique<ExpressionFormula>(cola::copy(*formula.expr));
	}

	void visit(const NegatedFormula& formula) override {
		formula.formula->accept(*this);
		result_formula = std::make_unique<NegatedFormula>(std::move(result_formula));
	}

	void visit(const OwnershipFormula& formula) override {
		result_formula = std::make_unique<OwnershipFormula>(std::make_unique<VariableExpression>(formula.expr->decl));
	}

	void visit(const LogicallyContainedFormula& formula) override {
		result_formula = std::make_unique<LogicallyContainedFormula>(cola::copy(*formula.expr));
	}

	void visit(const FlowFormula& formula) override {
		result_formula = std::make_unique<FlowFormula>(cola::copy(*formula.expr), formula.flow);
	}

	void visit(const ObligationFormula& /*formula*/) override {
		throw std::logic_error("not yet implemented: plankton::copy(const ObligationFormula&)");
	}

	void visit(const FulfillmentFormula& /*formula*/) override {
		throw std::logic_error("not yet implemented: plankton::copy(const FulfillmentFormula&)");
	}

	void visit(const PastPredicate& predicate) override {
		predicate.formula->accept(*this);
		result_predicate = std::make_unique<PastPredicate>(std::move(result_formula));
	}

	void visit(const FuturePredicate& predicate) override {
		predicate.pre->accept(*this);
		auto pre = std::move(result_formula);
		predicate.post->accept(*this);
		auto post = std::move(result_formula);
		auto cmd = std::make_unique<Assignment>(cola::copy(*predicate.command->lhs), cola::copy(*predicate.command->rhs));
		result_predicate = std::make_unique<FuturePredicate>(std::move(pre), std::move(cmd), std::move(post));
	}

	void visit(const Annotation& annotation) override {
		annotation.now->accept(*this);
		auto copy = std::make_unique<Annotation>(std::move(result_formula));
		for (const auto& pred : annotation.time) {
			pred->accept(*this);
			copy->time.push_back(std::move(result_predicate));
		}
		result_annotation = std::move(copy);
	}
};

std::unique_ptr<Formula> plankton::copy(const Formula& formula) {
	return std::move(CopyVisitor(formula).result_formula);
}
std::unique_ptr<TimePredicate> plankton::copy(const TimePredicate& predicate) {
	return std::move(CopyVisitor(predicate).result_predicate);
}
std::unique_ptr<Annotation> plankton::copy(const Annotation& annotation) {
	return std::move(CopyVisitor(annotation).result_annotation);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// struct FlattenVisitor : public LogicNonConstVisitor {
// 	std::unique_ptr<ConjunctionFormula> result;
// 	FlattenVisitor() : result(std::make_unique<ConjunctionFormula>()) {}

// 	virtual void visit(ConjunctionFormula& formula) = 0;
// 	virtual void visit(ImplicationFormula& formula) = 0;
// 	virtual void visit(ExpressionFormula& formula) = 0;
// 	virtual void visit(NegatedFormula& formula) = 0;
// 	virtual void visit(OwnershipFormula& formula) = 0;
// 	virtual void visit(LogicallyContainedFormula& formula) = 0;
// 	virtual void visit(FlowFormula& formula) = 0;
// 	virtual void visit(ObligationFormula& formula) = 0;
// 	virtual void visit(FulfillmentFormula& formula) = 0;
// 	virtual void visit(PastPredicate& formula) = 0;
// 	virtual void visit(FuturePredicate& formula) = 0;
// 	virtual void visit(Annotation& formula) = 0;
// };

std::unique_ptr<ConjunctionFormula> plankton::flatten(std::unique_ptr<Formula> /*formula*/) {
	throw std::logic_error("not yet implemented (plankton::flatten)");
}

std::unique_ptr<ConjunctionFormula> plankton::flatten(const Formula& formula) {
	return plankton::flatten(plankton::copy(formula));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


bool plankton::syntactically_equal(const Expression& expression, const Expression& other) {
	// TODO: find a better way to do this
	// NOTE: this relies on two distinct VariableDeclaration objects not having the same name
	std::stringstream expressionStream, otherStream;
	cola::print(expression, expressionStream);
	cola::print(other, otherStream);
	return expressionStream.str() == otherStream.str();
}


bool plankton::contains_expression(const Formula& /*formula*/, const Expression& /*search*/) {
	throw std::logic_error("not yet implemented: plankton::contains_expression");
}

bool plankton::contains_expression(const TimePredicate& /*predicate*/, const Expression& /*search*/) {
	throw std::logic_error("not yet implemented: plankton::contains_expression");
}

// struct InterferneceExpressionRenamer : public BaseNonConstVisitor {
// 	RenamingInfo& info;
// 	std::unique_ptr<Expression>* current_owner = nullptr;

// 	InterferneceExpressionRenamer(RenamingInfo& info_) : info(info_) {}

// 	void handle_expression(std::unique_ptr<Expression>& expr) {
// 		current_owner = &expr;
// 		expr->accept(*this);
// 	}

// 	std::unique_ptr<VariableExpression> handle_variable(VariableExpression& expr) {
// 		return std::make_unique<VariableExpression>(info.rename(expr.decl));
// 	}

// 	void visit(VariableExpression& expr) override {
// 		assert(current_owner);
// 		assert(current_owner->get() == &expr);
// 		*current_owner = handle_variable(expr);
// 	}
	
// 	void visit(Dereference& expr) override {
// 		handle_expression(expr.expr);
// 	}
	
// 	void visit(NegatedExpression& expr) override {
// 		handle_expression(expr.expr);
// 	}

// 	void visit(BinaryExpression& expr) override {
// 		handle_expression(expr.lhs);
// 		handle_expression(expr.rhs);
// 	}

// 	void visit(BooleanValue& /*expr*/) override { /* do nothing */ }
// 	void visit(NullValue& /*expr*/) override { /* do nothing */ }
// 	void visit(EmptyValue& /*expr*/) override { /* do nothing */ }
// 	void visit(MaxValue& /*expr*/) override { /* do nothing */ }
// 	void visit(MinValue& /*expr*/) override { /* do nothing */ }
// 	void visit(NDetValue& /*expr*/) override { /* do nothing */ }
// };

// struct InterferenceFormulaRenamer : LogicNonConstVisitor {
// 	InterferneceExpressionRenamer expr_renamer;

// 	InterferenceFormulaRenamer(RenamingInfo& info_) : expr_renamer(info_) {}

// 	void visit(ConjunctionFormula& formula) override {
// 		for (auto& conjunct : formula.conjuncts) {
// 			conjunct->accept(*this);
// 		}
// 	}

// 	void visit(ExpressionFormula& formula) override {
// 		expr_renamer.handle_expression(formula.expr);
// 	}

// 	void visit(NegatedFormula& formula) override {
// 		formula.formula->accept(*this);
// 	}

// 	void visit(OwnershipFormula& formula) override {
// 		formula.expr = expr_renamer.handle_variable(*formula.expr);
// 	}

// 	void visit(LogicallyContainedFormula& formula) override {
// 		expr_renamer.handle_expression(formula.expr);
// 	}

// 	void visit(FlowFormula& formula) override {
// 		expr_renamer.handle_expression(formula.expr);
// 	}

// 	void visit(ObligationFormula& /*formula*/) override {
// 		throw std::logic_error("not yet implemented: InterferenceFormulaRenamer::visit(ObligationFormula&)");
// 	}

// 	void visit(FulfillmentFormula& /*formula*/) override {
// 		throw std::logic_error("not yet implemented: InterferenceFormulaRenamer::visit(FulfillmentFormula&)");
// 	}

// 	void visit(PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const PastPredicate&)"); }
// 	void visit(FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const FuturePredicate&)"); }
// 	void visit(Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: InterferenceFormulaRenamer::visit(const Annotation&)"); }
// };


std::unique_ptr<Expression> plankton::replace_expression(std::unique_ptr<Expression> /*expression*/, transformer_t /*transformer*/) {
	throw std::logic_error("not yet implemented: plankton::replace_expression");
}

std::unique_ptr<Formula> plankton::replace_expression(std::unique_ptr<Formula> /*formula*/, transformer_t /*transformer*/) {
	throw std::logic_error("not yet implemented: plankton::replace_expression");
}

std::unique_ptr<TimePredicate> plankton::replace_expression(std::unique_ptr<TimePredicate> /*predicate*/, transformer_t /*transformer*/) {
	throw std::logic_error("not yet implemented: plankton::replace_expression");
}


transformer_t make_transformer(const Expression& replace, const Expression& with) {
	return [&] (const auto& expr) {
		if (syntactically_equal(expr, replace)) {
			return std::make_pair(true, cola::copy(with));
		} else {
			return std::make_pair(false, std::unique_ptr<Expression>());
		}
	};
}

std::unique_ptr<Expression> plankton::replace_expression(std::unique_ptr<Expression> expression, const cola::Expression& replace, const cola::Expression& with) {
	return plankton::replace_expression(std::move(expression), make_transformer(replace, with));
}

std::unique_ptr<Formula> plankton::replace_expression(std::unique_ptr<Formula> formula, const Expression& replace, const Expression& with) {
	return plankton::replace_expression(std::move(formula), make_transformer(replace, with));
}

std::unique_ptr<TimePredicate> plankton::replace_expression(std::unique_ptr<TimePredicate> predicate, const Expression& replace, const Expression& with) {
	return plankton::replace_expression(std::move(predicate), make_transformer(replace, with));
}
