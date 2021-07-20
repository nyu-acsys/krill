#include "heal/util.hpp"

#include <sstream>

using namespace cola;
using namespace heal;


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
			if (node.op == other.op && is_equal(*node.lhs, *other.lhs) && is_equal(*node.rhs, *other.rhs)) return true;
			if (node.op == cola::symmetricOp(other.op) && is_equal(*node.lhs, *other.rhs) && is_equal(*node.rhs, *other.lhs)) return true;
			return false;
		});
	}

	void visit(const Dereference& node) override {
		return handle(node, [this](const auto& node, const auto& other){
			return node.fieldname == other.fieldname && is_equal(*node.expr, *other.expr);
		});
	}
};

bool heal::SyntacticallyEqual(const Expression& expression, const Expression& other) {
	static SyntacticEqualityChecker visitor;
	return visitor.is_equal(expression, other);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct LogicSyntacticEqualityChecker : public LogicVisitor {
	const LogicObject* compare_to;
	bool result;
	
	bool is_equal(const LogicObject& formula, const LogicObject& other) {
		compare_to = &other;
		result = false;
		formula.accept(*this);
		return result;
	}
	
	template<typename T>
	bool is_equal(const std::deque<std::unique_ptr<T>>& elems, const std::deque<std::unique_ptr<T>>& other) {
		for (const auto& elem : elems) {
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

	void visit(const SymbolicVariable& formula) override {
		handle(formula, [](const auto& expr, const auto& other){
			return &expr.Decl() == &other.Decl();
		});
	}

	void visit(const SymbolicBool& formula) override {
		handle(formula, [](const auto& expr, const auto& other){
			return expr.value == other.value;
		});
	}

	void visit(const SymbolicNull& formula) override {
		handle(formula, [](const auto&, const auto&){
			return true;
		});
	}

	void visit(const SymbolicMin& formula) override {
		handle(formula, [](const auto&, const auto&){
			return true;
		});
	}

	void visit(const SymbolicMax& formula) override {
		handle(formula, [](const auto&, const auto&){
			return true;
		});
	}



	void visit(const SeparatingConjunction& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(formula.conjuncts, other.conjuncts);
		});
	}

	void visit(const FlatSeparatingConjunction& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(formula.conjuncts, other.conjuncts);
		});
	}

	void visit(const SeparatingImplication& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.premise, *other.premise) && is_equal(*formula.conclusion, *other.conclusion);
		});
	}

	void visit(const NegatedAxiom& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.axiom, *other.axiom);
		});
	}

	void visit(const BoolAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return formula.value == other.value;
		});
	}

	void visit(const PointsToAxiom& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return formula.fieldname == other.fieldname && is_equal(*formula.node, *other.node) && is_equal(*formula.value, *other.value);
		});
	}

	void visit(const StackAxiom& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return formula.op == other.op && is_equal(*formula.lhs, *other.lhs) && is_equal(*formula.rhs, *other.rhs);
		});
	}

	void visit(const StackDisjunction& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(formula.axioms, other.axioms);
		});
	}

	void visit(const OwnershipAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return heal::SyntacticallyEqual(*formula.node, *other.node);
		});
	}

	void visit(const DataStructureLogicallyContainsAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return heal::SyntacticallyEqual(*formula.value, *other.value);
		});
	}

	void visit(const NodeLogicallyContainsAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return heal::SyntacticallyEqual(*formula.node, *other.node) &&
                   heal::SyntacticallyEqual(*formula.value, *other.value);
		});
	}

	void visit(const KeysetContainsAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return heal::SyntacticallyEqual(*formula.node, *other.node) &&
                   heal::SyntacticallyEqual(*formula.value, *other.value);
		});
	}

	void visit(const HasFlowAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return heal::SyntacticallyEqual(*formula.node, *other.node);
		});
	}

	void visit(const FlowContainsAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return heal::SyntacticallyEqual(*formula.node, *other.node)
			    && heal::SyntacticallyEqual(*formula.value_low, *other.value_low)
			    && heal::SyntacticallyEqual(*formula.value_high, *other.value_high);
		});
	}

	void visit(const UniqueInflowAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return heal::SyntacticallyEqual(*formula.node, *other.node);
		});
	}

	void visit(const ObligationAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return formula.kind == other.kind && heal::SyntacticallyEqual(*formula.key, *other.key);
		});
	}

	void visit(const FulfillmentAxiom& formula) override {
		handle(formula, [](const auto& formula, const auto& other){
			return formula.kind == other.kind && formula.return_value == other.return_value &&
                    heal::SyntacticallyEqual(*formula.key, *other.key);
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
			    && heal::SyntacticallyEqual(*formula.command->lhs, *other.command->lhs)
			    && heal::SyntacticallyEqual(*formula.command->rhs, *other.command->rhs);
		});
	}

	void visit(const Annotation& formula) override {
		handle(formula, [this](const auto& formula, const auto& other){
			return is_equal(*formula.now, *other.now) && is_equal(formula.time, other.time);
		});
	}

};

bool heal::SyntacticallyEqual(const LogicObject& object, const LogicObject& other) {
	static LogicSyntacticEqualityChecker visitor;
	return visitor.is_equal(object, other);
}
