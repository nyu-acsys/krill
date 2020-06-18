#include "plankton/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


struct ExpressionReplacer : public BaseNonConstVisitor, public DefaultNonConstListener {
	using BaseNonConstVisitor::visit;
	using DefaultNonConstListener::visit;
	using DefaultNonConstListener::enter;

	transformer_t transformer;
	ExpressionReplacer(transformer_t transformer_) : transformer(transformer_) {}

	template<typename T>
	static std::unique_ptr<T> replace(std::unique_ptr<T> obj, transformer_t transformer) {
		ExpressionReplacer visitor(transformer);
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

	void enter(FlowAxiom& formula) override {
		handle_expression(formula.expr);
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


std::unique_ptr<Expression> plankton::replace_expression(std::unique_ptr<Expression> expression, transformer_t transformer) {
	return ExpressionReplacer::replace(std::move(expression), transformer);
}

template<typename F, typename X>
std::unique_ptr<F> plankton::replace_expression(std::unique_ptr<F> formula, transformer_t transformer) {
	return ExpressionReplacer::replace(std::move(formula), transformer);
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

template<typename F, typename X>
std::unique_ptr<F> plankton::replace_expression(std::unique_ptr<F> formula, const Expression& replace, const Expression& with) {
	return plankton::replace_expression(std::move(formula), make_transformer(replace, with));
}
