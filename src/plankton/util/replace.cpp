#include "plankton/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


struct ExpressionReplacer : public BaseNonConstVisitor, public DefaultNonConstListener {
	using BaseNonConstVisitor::visit;
	using DefaultNonConstListener::visit;
	using DefaultNonConstListener::enter;

	const transformer_t& transformer;
	bool performedUpdate = false;
	ExpressionReplacer(const transformer_t& transformer_) : transformer(transformer_) {}

	template<typename T>
	static std::pair<std::unique_ptr<T>, bool> replace(std::unique_ptr<T> obj, transformer_t transformer) {
		ExpressionReplacer visitor(transformer);
		obj->accept(visitor);
		return { std::move(obj), visitor.performedUpdate };
	}

	void handle_expression(std::unique_ptr<Expression>& expression) {
		auto [replace, replacement] = transformer(*expression);
		if (replace) {
			expression = std::move(replacement);
		} else {
			expression->accept(*this);
		}
	}

	void handle_expression(std::unique_ptr<VariableExpression>& expression) {
		auto [replace, replacement] = transformer(*expression);
		if (replace) {
			Expression* raw = replacement.get();
			VariableExpression* cast = dynamic_cast<VariableExpression*>(raw);
			if (!cast) {
				throw std::logic_error("Replacement violates AST: required replacement for 'VariableExpression' to be of type 'VariableExpression'.");
			}
			replacement.release();
			expression.reset(cast);
		} else {
			expression->accept(*this);
		}
	}
	
	void visit(NegatedExpression& node) override {
		handle_expression(node.expr);
	}

	void visit(BinaryExpression& node) override {
		handle_expression(node.lhs);
		handle_expression(node.rhs);
	}

	void visit(Dereference& node) override {
		handle_expression(node.expr);
	}

	void visit(BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(NullValue& /*node*/) override { /* do nothing */ }
	void visit(EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(MaxValue& /*node*/) override { /* do nothing */ }
	void visit(MinValue& /*node*/) override { /* do nothing */ }
	void visit(NDetValue& /*node*/) override { /* do nothing */ }
	void visit(VariableExpression& /*node*/) override { /* do nothing */ }

	void enter(ExpressionAxiom& formula) override {
		handle_expression(formula.expr);
	}

	void enter(OwnershipAxiom& formula) override {
		handle_expression(formula.expr);
	}

	void enter(LogicallyContainedAxiom& formula) override {
		handle_expression(formula.expr);
	}

	void enter(KeysetContainsAxiom& formula) override {
		handle_expression(formula.node);
		handle_expression(formula.value);
	}

	void enter(HasFlowAxiom& formula) override {
		handle_expression(formula.expr);
	}

	void enter(FlowContainsAxiom& formula) override {
		handle_expression(formula.expr);
		handle_expression(formula.low_value);
		handle_expression(formula.high_value);
	}

	void enter(ObligationAxiom& formula) override {
		handle_expression(formula.key);
	}

	void enter(FulfillmentAxiom& formula) override {
		handle_expression(formula.key);
	}

	void enter(FuturePredicate& formula) override {
		handle_expression(formula.command->lhs);
		handle_expression(formula.command->rhs);
	}
};


template<typename T>
std::unique_ptr<T> make_replacement(std::unique_ptr<T> obj, const transformer_t& transformer, bool* changed) {
	auto [result, performedUpdate] = ExpressionReplacer::replace(std::move(obj), transformer);
	if (changed) *changed = performedUpdate;
	return std::move(result);
}

std::unique_ptr<Expression> plankton::replace_expression(std::unique_ptr<Expression> expression, const transformer_t& transformer, bool* changed) {
	return make_replacement(std::move(expression), transformer, changed);
}

std::unique_ptr<Formula> plankton::replace_expression(std::unique_ptr<Formula> formula, const transformer_t& transformer, bool* changed) {
	return make_replacement(std::move(formula), transformer, changed);
}

std::unique_ptr<SimpleFormula> plankton::replace_expression(std::unique_ptr<SimpleFormula> formula, const transformer_t& transformer, bool* changed) {
	return make_replacement(std::move(formula), transformer, changed);
}

std::unique_ptr<Axiom> plankton::replace_expression(std::unique_ptr<Axiom> formula, const transformer_t& transformer, bool* changed) {
	return make_replacement(std::move(formula), transformer, changed);
}

std::unique_ptr<ConjunctionFormula> plankton::replace_expression(std::unique_ptr<ConjunctionFormula> formula, const transformer_t& transformer, bool* changed) {
	return make_replacement(std::move(formula), transformer, changed);
}

std::unique_ptr<TimePredicate> plankton::replace_expression(std::unique_ptr<TimePredicate> formula, const transformer_t& transformer, bool* changed) {
	return make_replacement(std::move(formula), transformer, changed);
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

std::unique_ptr<Expression> plankton::replace_expression(std::unique_ptr<Expression> expression, const cola::Expression& replace, const cola::Expression& with, bool* changed) {
	return plankton::replace_expression(std::move(expression), make_transformer(replace, with), changed);
}

std::unique_ptr<Formula> plankton::replace_expression(std::unique_ptr<Formula> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed) {
	return plankton::replace_expression(std::move(formula), make_transformer(replace, with), changed);
}

std::unique_ptr<Axiom> plankton::replace_expression(std::unique_ptr<Axiom> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed) {
	return plankton::replace_expression(std::move(formula), make_transformer(replace, with), changed);
}

std::unique_ptr<SimpleFormula> plankton::replace_expression(std::unique_ptr<SimpleFormula> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed) {
	return plankton::replace_expression(std::move(formula), make_transformer(replace, with), changed);
}

std::unique_ptr<ConjunctionFormula> plankton::replace_expression(std::unique_ptr<ConjunctionFormula> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed) {
	return plankton::replace_expression(std::move(formula), make_transformer(replace, with), changed);
}

std::unique_ptr<TimePredicate> plankton::replace_expression(std::unique_ptr<TimePredicate> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed) {
	return plankton::replace_expression(std::move(formula), make_transformer(replace, with), changed);
}
