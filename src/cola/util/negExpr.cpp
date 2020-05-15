#include "cola/util.hpp"

using namespace cola;


BinaryExpression::Operator negate_binary_opereator(BinaryExpression::Operator op) {
	switch (op) {
		case BinaryExpression::Operator::EQ: return BinaryExpression::Operator::NEQ;
		case BinaryExpression::Operator::NEQ: return BinaryExpression::Operator::EQ;
		case BinaryExpression::Operator::LEQ: return BinaryExpression::Operator::GT;
		case BinaryExpression::Operator::LT: return BinaryExpression::Operator::GEQ;
		case BinaryExpression::Operator::GEQ: return BinaryExpression::Operator::LT;
		case BinaryExpression::Operator::GT: return BinaryExpression::Operator::LEQ;
		case BinaryExpression::Operator::AND: return BinaryExpression::Operator::OR;
		case BinaryExpression::Operator::OR: return BinaryExpression::Operator::AND;
	}
}

struct NegateExpressionVisitor final : public BaseVisitor {
	std::unique_ptr<Expression> result;

	void visit(const BooleanValue& expr) {
		assert(!result);
		result = std::make_unique<BooleanValue>(!expr.value);
	}

	void visit(const NegatedExpression& expr) {
		assert(!result);
		result = cola::copy(*expr.expr);
	}

	void visit(const BinaryExpression& expr) {
		assert(!result);
		auto inverted_op = negate_binary_opereator(expr.op);
		if (inverted_op == BinaryExpression::Operator::AND || inverted_op == BinaryExpression::Operator::OR) {
			expr.lhs->accept(*this);
			auto lhs = std::move(result);
			expr.rhs->accept(*this);
			auto rhs = std::move(result);
			result = std::make_unique<BinaryExpression>(inverted_op, std::move(lhs), std::move(rhs));
		} else {
			result = std::make_unique<BinaryExpression>(inverted_op, cola::copy(*expr.lhs), cola::copy(*expr.rhs));
		}
	}

	void visit(const VariableExpression& expr) {
		assert(expr.decl.type.sort == Sort::BOOL);
		assert(!result);
		result = std::make_unique<NegatedExpression>(cola::copy(expr));
	}
};

std::unique_ptr<Expression> cola::negate(const Expression& expr) {
		NegateExpressionVisitor visitor;
		expr.accept(visitor);
		return std::move(visitor.result);
}
