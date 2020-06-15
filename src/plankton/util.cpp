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

struct FlattenVisitor : public LogicNonConstVisitor {
	std::unique_ptr<ConjunctionFormula> result;
	bool drop_child = false;
	FlattenVisitor() : result(std::make_unique<ConjunctionFormula>()) {}

	void visit(ConjunctionFormula& formula) override {
		for (auto& conjunct : formula.conjuncts) {
			drop_child = false;
			conjunct->accept(*this);
			if (!drop_child) {
				result->conjuncts.push_back(std::move(conjunct));
			}
		}
		drop_child = true;
	}

	void visit(NegatedFormula& formula) override {
		Formula* raw = formula.formula.get();
		ImplicationFormula* cast = dynamic_cast<ImplicationFormula*>(raw);
		if (cast) {
			auto premise = plankton::flatten(std::move(cast->premise));
			result->conjuncts.insert(result->conjuncts.end(), std::make_move_iterator(premise->conjuncts.begin()), std::make_move_iterator(premise->conjuncts.end()));
			auto conclusion = std::move(cast->conclusion);
			result->conjuncts.push_back(std::make_unique<NegatedFormula>(std::move(conclusion)));
			drop_child = true;

		} else {
			/* do nothing */
		}
	}

	void visit(ImplicationFormula& /*formula*/) override { /* do nothing */ }
	void visit(ExpressionFormula& /*formula*/) override { /* do nothing */ }
	void visit(OwnershipFormula& /*formula*/) override { /* do nothing */ }
	void visit(LogicallyContainedFormula& /*formula*/) override { /* do nothing */ }
	void visit(FlowFormula& /*formula*/) override { /* do nothing */ }
	void visit(ObligationFormula& /*formula*/) override { /* do nothing */ }
	void visit(FulfillmentFormula& /*formula*/) override { /* do nothing */ }
	
	void visit(PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: FlattenVisitor::visit(PastPredicate&)"); }
	void visit(FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: FlattenVisitor::visit(FuturePredicate&)"); }
	void visit(Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: FlattenVisitor::visit(Annotation&)"); }
};

std::unique_ptr<ConjunctionFormula> plankton::flatten(std::unique_ptr<Formula> formula) {
	 // we need a conjunction formula at top level, create a dummy one
	auto dummy = std::make_unique<ConjunctionFormula>();
	dummy->conjuncts.push_back(std::move(formula));

	// flatten the dummy
	FlattenVisitor visitor;
	dummy->accept(visitor);
	return std::move(visitor.result);
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

struct ContainsVisitor : public BaseVisitor, public LogicVisitor {
	using BaseVisitor::visit;
	using LogicVisitor::visit;

	const Expression& search;
	bool found = false;
	ContainsVisitor(const Expression& search_) : search(search_) {}

	template<typename T>
	static bool contains(const T& obj, const Expression& search) {
		ContainsVisitor visitor(search);
		obj.accept(visitor);
		return visitor.found;
	}

	template<typename T>
	void handle_expression(const std::unique_ptr<T>& expression) {
		if (syntactically_equal(*expression, search)) {
			found = true;
		}
		if (!found) {
			expression->accept(*this);
		}
	}

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }
	void visit(const NegatedExpression& node) override { handle_expression(node.expr); }
	void visit(const BinaryExpression& node) override { handle_expression(node.lhs); handle_expression(node.rhs); }
	void visit(const Dereference& node) override { handle_expression(node.expr); }
	void visit(const ExpressionFormula& formula) override { handle_expression(formula.expr); }
	void visit(const OwnershipFormula& formula) override { handle_expression(formula.expr); }
	void visit(const LogicallyContainedFormula& formula) override { handle_expression(formula.expr); }
	void visit(const FlowFormula& formula) override { handle_expression(formula.expr); }
	void visit(const ObligationFormula& /*formula*/) override { throw std::logic_error("not yet implemented: ContainsVisitor::visit(const ObligationFormula&)"); }
	void visit(const FulfillmentFormula& /*formula*/) override { throw std::logic_error("not yet implemented: ContainsVisitor::visit(const FulfillmentFormula&)"); }
	void visit(const NegatedFormula& formula) override { formula.formula->accept(*this); }
	void visit(const ConjunctionFormula& formula) override {
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}
	void visit(const ImplicationFormula& formula) override { formula.premise->accept(*this); formula.conclusion->accept(*this); }
	void visit(const PastPredicate& formula) override { formula.formula->accept(*this); }
	void visit(const FuturePredicate& formula) override {
		formula.pre->accept(*this);
		handle_expression(formula.command->lhs);
		handle_expression(formula.command->rhs);
		formula.post->accept(*this);
	}
	void visit(const Annotation& formula) override {
		formula.now->accept(*this);
		for (const auto& pred : formula.time) {
			pred->accept(*this);
		}
	}
};

bool plankton::contains_expression(const Formula& formula, const Expression& search) {
	return ContainsVisitor::contains(formula, search);
}

bool plankton::contains_expression(const TimePredicate& predicate, const Expression& search) {
	return ContainsVisitor::contains(predicate, search);
}


struct ReplacementVisitor : public BaseNonConstVisitor, public LogicNonConstVisitor {
	using BaseNonConstVisitor::visit;
	using LogicNonConstVisitor::visit;

	transformer_t transformer;
	ReplacementVisitor(transformer_t transformer_) : transformer(transformer_) {}

	template<typename T>
	static std::unique_ptr<T> replace(std::unique_ptr<T> obj, transformer_t transformer) {
		ReplacementVisitor visitor(transformer);
		obj->accept(visitor);
		return std::move(obj);
	}

	void handle_expression(std::unique_ptr<Expression>& expression) {
		auto [replace, replacement] = transformer(*expression);
		if (replace) {
			expression = std::move(replacement);
		} else {
			expression->accept(*this);
		}
	}

	void visit(OwnershipFormula& formula) override {
		auto [replace, replacement] = transformer(*formula.expr);
		if (replace) {
			Expression* raw = replacement.get();
			VariableExpression* cast = dynamic_cast<VariableExpression*>(raw);
			if (!cast) {
				throw std::logic_error("Replacement violates AST: replacing 'VariableExpression' in 'OwnershipFormula' with an incompatible type.");
			}
			formula.expr.release();
			formula.expr.reset(cast);
		} else {
			formula.expr->accept(*this);
		}
	}

	void visit(BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(NullValue& /*node*/) override { /* do nothing */ }
	void visit(EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(MaxValue& /*node*/) override { /* do nothing */ }
	void visit(MinValue& /*node*/) override { /* do nothing */ }
	void visit(NDetValue& /*node*/) override { /* do nothing */ }
	void visit(VariableExpression& /*node*/) override { /* do nothing */ }
	void visit(NegatedExpression& node) override { handle_expression(node.expr); }
	void visit(BinaryExpression& node) override { handle_expression(node.lhs); handle_expression(node.rhs); }
	void visit(Dereference& node) override { handle_expression(node.expr); }
	void visit(ExpressionFormula& formula) override { handle_expression(formula.expr); }
	void visit(LogicallyContainedFormula& formula) override { handle_expression(formula.expr); }
	void visit(FlowFormula& formula) override { handle_expression(formula.expr); }
	void visit(ObligationFormula& /*formula*/) override { throw std::logic_error("not yet implemented: ReplacementVisitor::visit(ObligationFormula&)"); }
	void visit(FulfillmentFormula& /*formula*/) override { throw std::logic_error("not yet implemented: ReplacementVisitor::visit(FulfillmentFormula&)"); }
	void visit(NegatedFormula& formula) override { formula.formula->accept(*this); }
	void visit(ConjunctionFormula& formula) override {
		for (auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}
	void visit(ImplicationFormula& formula) override { formula.premise->accept(*this); formula.conclusion->accept(*this); }
	void visit(PastPredicate& formula) override { formula.formula->accept(*this); }
	void visit(FuturePredicate& formula) override {
		formula.pre->accept(*this);
		handle_expression(formula.command->lhs);
		handle_expression(formula.command->rhs);
		formula.post->accept(*this);
	}
	void visit(Annotation& formula) override {
		formula.now->accept(*this);
		for (auto& pred : formula.time) {
			pred->accept(*this);
		}
	}
};


std::unique_ptr<Expression> plankton::replace_expression(std::unique_ptr<Expression> expression, transformer_t transformer) {
	return ReplacementVisitor::replace(std::move(expression), transformer);
}

std::unique_ptr<Formula> plankton::replace_expression(std::unique_ptr<Formula> formula, transformer_t transformer) {
	return ReplacementVisitor::replace(std::move(formula), transformer);
}

std::unique_ptr<TimePredicate> plankton::replace_expression(std::unique_ptr<TimePredicate> predicate, transformer_t transformer) {
	return ReplacementVisitor::replace(std::move(predicate), transformer);
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
