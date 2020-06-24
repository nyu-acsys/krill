#include "plankton/util.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


struct SyntacticEqualityChecker : public BaseVisitor {
	const Expression* compare_to;
	bool result;
	
	bool is_equal(const Expression& expression, const Expression& other) {
		compare_to = &other;
		result = false;
		expression.accept(*this);
		return result;
	}

	template<typename T, typename F = std::function<bool(const T&, const T&)>>
	inline void handle(const T& expression, F chk_equal) {
		if (auto other = dynamic_cast<const T*>(compare_to)) {
			result = chk_equal(expression, *other);
		}
	}

	void visit(const VariableDeclaration& node) override {
		return handle(node, [](const auto& node, const auto& other){
			return &node == &other;
		});
	}

	void visit(const BooleanValue& node) override {
		return handle(node, [](const auto& node, const auto& other){
			return node.value == other.value;
		});
	}

	void visit(const NullValue& node) override {
		return handle(node, [](const auto& /*node*/, const auto& /*other*/){
			return true;
		});
	}

	void visit(const EmptyValue& node) override {
		return handle(node, [](const auto& /*node*/, const auto& /*other*/){
			return true;
		});
	}

	void visit(const MaxValue& node) override {
		return handle(node, [](const auto& /*node*/, const auto& /*other*/){
			return true;
		});
	}

	void visit(const MinValue& node) override {
		return handle(node, [](const auto& /*node*/, const auto& /*other*/){
			return true;
		});
	}

	void visit(const NDetValue& node) override {
		return handle(node, [](const auto& /*node*/, const auto& /*other*/){
			return true;
		});
	}

	void visit(const VariableExpression& node) override {
		return handle(node, [](const auto& node, const auto& other){
			return &node.decl == &other.decl;
		});
	}

	void visit(const NegatedExpression& node) override {
		return handle(node, [this](const auto& node, const auto& other){
			return is_equal(*node.expr, *other.expr);
		});
	}

	void visit(const BinaryExpression& node) override {
		return handle(node, [this](const auto& node, const auto& other){
			if (node.op != other.op) return false;
			if (is_equal(*node.lhs, *other.lhs) && is_equal(*node.rhs, *other.rhs)) return true;
			if (is_equal(*node.lhs, *other.rhs) && is_equal(*node.rhs, *other.lhs)) return true;
			return false;
		});
	}

	void visit(const Dereference& node) override {
		return handle(node, [this](const auto& node, const auto& other){
			return node.fieldname == other.fieldname && is_equal(*node.expr, *other.expr);
		});
	}
};

bool plankton::syntactically_equal(const Expression& expression, const Expression& other) {
	static SyntacticEqualityChecker visitor;
	return visitor.is_equal(expression, other);

	// // // TODO: use the following code for testing?
	// // NOTE: this relies on two distinct VariableDeclaration objects not having the same name
	// std::stringstream expressionStream, otherStream;
	// cola::print(expression, expressionStream);
	// cola::print(other, otherStream);
	// return expressionStream.str() == otherStream.str();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct LogicSyntacticEqualityChecker : public LogicVisitor {
	const Formula* compare_to;
	bool result;
	
	bool is_equal(const Formula& formula, const Formula& other) {
		compare_to = &other;
		result = false;
		formula.accept(*this);
		return result;
	}
	
	template<typename T>
	bool is_equal(const std::deque<std::unique_ptr<T>>& conjuncts, const std::deque<std::unique_ptr<T>>& other) {
		for (const auto& elem : conjuncts) {
			bool has_match = false;
			for (const auto& check : other) {
				if (is_equal(*elem, *check)) {
					has_match = true;
					break;
				}
			}
			if (!has_match) {
				return false;
			}
		}
		return true;
	}

	template<typename T, typename F = std::function<bool(const T&, const T&)>>
	inline void handle(const T& formula, const F& chk_equal) {
		if (auto other = dynamic_cast<const T*>(compare_to)) {
			result = chk_equal(formula, *other);
		}
	}

	void visit(const AxiomConjunctionFormula& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(formula.conjuncts, other.conjuncts);
		});
	}

	void visit(const ConjunctionFormula& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(formula.conjuncts, other.conjuncts);
		});
	}

	void visit(const ImplicationFormula& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.premise, *other.premise) && is_equal(*formula.conclusion, *other.conclusion);
		});
	}

	void visit(const NegatedAxiom& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.axiom, *other.axiom);
		});
	}

	void visit(const ExpressionAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return plankton::syntactically_equal(*formula.expr, *other.expr);
		});
	}

	void visit(const OwnershipAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return plankton::syntactically_equal(*formula.expr, *other.expr);
		});
	}

	void visit(const LogicallyContainedAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return plankton::syntactically_equal(*formula.expr, *other.expr);
		});
	}

	void visit(const KeysetContainsAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return plankton::syntactically_equal(*formula.node, *other.node) && plankton::syntactically_equal(*formula.value, *other.value);
		});
	}

	void visit(const FlowAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return plankton::syntactically_equal(*formula.expr, *other.expr) && is_flow_equal(formula.flow, other.flow);
		});
	}

	void visit(const ObligationAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return formula.kind == other.kind && plankton::syntactically_equal(*formula.key, *other.key);
		});
	}

	void visit(const FulfillmentAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return formula.kind == other.kind && formula.return_value == other.return_value && plankton::syntactically_equal(*formula.key, *other.key);
		});
	}

	void visit(const PastPredicate& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.formula, *other.formula);
		});
	}

	void visit(const FuturePredicate& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.pre, *other.pre) && is_equal(*formula.post, *other.post)
			    && plankton::syntactically_equal(*formula.command->lhs, *other.command->lhs)
			    && plankton::syntactically_equal(*formula.command->rhs, *other.command->rhs);
		});
	}

	void visit(const Annotation& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.now, *other.now) && is_equal(formula.time, other.time);
		});
	}

};

bool plankton::syntactically_equal(const SimpleFormula& formula, const SimpleFormula& other) {
	static LogicSyntacticEqualityChecker visitor;
	return visitor.is_equal(formula, other);

	// // TODO: use the following code for testing?
	// // NOTE: this relies on two distinct VariableDeclaration objects not having the same name
	// std::stringstream formulaStream, otherStream;
	// plankton::print(formula, formulaStream);
	// plankton::print(other, otherStream);
	// return formulaStream.str() == otherStream.str();
}