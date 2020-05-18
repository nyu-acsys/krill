#include "plankton/verify.hpp"

#include <deque>
#include "cola/visitors.hpp"
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


struct ConjunctionExtractor : public BaseNonConstVisitor {
	std::vector<std::unique_ptr<Expression>> conjunctions;

	void visit(BooleanValue& /*node*/) { /* do nothing */ }
	void visit(VariableExpression& /*node*/) { /* do nothing */ }
	void visit(NegatedExpression& /*node*/) { /* do nothing */ }
	void visit(Dereference& /*node*/) { /* do nothing */ }

	void visit(BinaryExpression& expr) {
		if (expr.op == BinaryExpression::Operator::AND) {
			conjunctions.push_back(std::move(expr.lhs));
			conjunctions.push_back(std::move(expr.rhs));
		}
	}

	std::vector<std::unique_ptr<Expression>> extract(Expression& expr) {
		conjunctions.clear();
		expr.accept(*this);
		return std::move(conjunctions);
	}
};

std::vector<std::unique_ptr<Expression>> extract_conjunts(std::unique_ptr<Expression> expression) {
	std::vector<std::unique_ptr<Expression>> result;
	ConjunctionExtractor extractor;
	
	std::deque<std::unique_ptr<Expression>> worklist;
	worklist.push_back(std::move(expression));
	while (!worklist.empty()) {
		std::unique_ptr<Expression> expr = std::move(worklist.front());
		worklist.pop_front();

		auto subexprs = extractor.extract(*expr);
		if (subexprs.empty()) {
			result.push_back(std::move(expr));
		} else {
			worklist.insert(worklist.end(), std::make_move_iterator(subexprs.begin()), std::make_move_iterator(subexprs.end()));
		}
	}

	return result;
}


void Formula::add_conjuncts(std::unique_ptr<Expression> expression) {
	auto conjuncts = extract_conjunts(std::move(expression));
	present.insert(present.end(), std::make_move_iterator(conjuncts.begin()), std::make_move_iterator(conjuncts.end()));
}

Formula Formula::copy() const {
	Formula result;
	for (const auto& elem : present) {
		result.present.push_back(cola::copy(*elem));
	}
	for (const auto& elem : history) {
		result.history.emplace_back(cola::copy(*elem.condition), cola::copy(*elem.expression));
	}
	for (const auto& elem : future) {
		result.future.emplace_back(cola::copy(*elem.condition), elem.command);
	}
	return result;
}

inline Formula mk_bool(bool value) {
	Formula result;
	result.present.push_back(std::make_unique<BooleanValue>(value));
	return result;
}

Formula Formula::make_true() {
	return mk_bool(true);
}

Formula Formula::make_false() {
	return mk_bool(false);
}

Formula Formula::make_from_expression(const Expression& expression) {
	Formula result;
	result.present.push_back(cola::copy(expression));
	return result;
}


bool plankton::implies(const Formula& /*formula*/, const Expression& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::implies(const Formula& /*formula*/, const Formula& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::is_equal(const Formula& /*formula*/, const Formula& /*other*/) {
	throw std::logic_error("not yet implemented (plankton::is_equal)");
}

Formula plankton::unify(const Formula& /*formula*/, const Formula& /*other*/) {
	// creates a formula that implies the passed formulas
	throw std::logic_error("not yet implemented (plankton::unify)");
}

Formula plankton::unify(const std::vector<Formula>& /*formulas*/) {
	// creates a formula that implies the passed formulas
	throw std::logic_error("not yet implemented (plankton::unify)");
} 
