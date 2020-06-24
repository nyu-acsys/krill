#include "plankton/util.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


bool plankton::syntactically_equal(const Expression& expression, const Expression& other) {
	// TODO: find a better way to do this
	// NOTE: this relies on two distinct VariableDeclaration objects not having the same name
	std::stringstream expressionStream, otherStream;
	cola::print(expression, expressionStream);
	cola::print(other, otherStream);
	return expressionStream.str() == otherStream.str();
}


struct LogicSyntacticEqualityChecker : public LogicVisitor {
	const Formula* compare_to;
	bool result;
	
	bool is_equal(const Formula& formula, const Formula& other) {
		compare_to = &other;
		result = false;
		formula.accept(*this);
		return result;
	}
	
	template<typename T> // T = TimePredicate, SimpleFormula, Axiom
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
	void handle(const T& formula, F chk_equal) {
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