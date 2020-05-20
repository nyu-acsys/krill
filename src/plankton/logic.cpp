#include "plankton/verify.hpp"

#include <deque>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


struct CopyVisitor : public FormulaVisitor {
	std::unique_ptr<Formula> result;

	std::unique_ptr<BasicFormula> result_as_basic_formula() {
		auto formula = dynamic_cast<BasicFormula*>(result.get());
		if (!formula) {
			throw std::logic_error("Could not copy formula.");
		}
		result.release();
		return std::unique_ptr<BasicFormula>(formula);
	}

	void visit(const ConjunctionFormula& formula) override {
		auto copy = std::make_unique<ConjunctionFormula>();
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
			copy->conjuncts.push_back(std::move(result));
		}
		result = std::move(copy);
	}

	void visit(const BasicConjunctionFormula& formula) override {
		auto copy = std::make_unique<BasicConjunctionFormula>();
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
			copy->conjuncts.push_back(result_as_basic_formula());
		}
		result = std::move(copy);
	}

	void visit(const ExpressionFormula& formula) override {
		result = std::make_unique<ExpressionFormula>(cola::copy(*formula.expr));
	}

	void visit(const HistoryFormula& formula) override {
		formula.condition->accept(*this);
		auto cond = result_as_basic_formula();
		formula.expression->accept(*this);
		auto expr = result_as_basic_formula();
		result = std::make_unique<HistoryFormula>(std::move(cond), std::move(expr));
	}

	void visit(const FutureFormula& formula) override {
		formula.condition->accept(*this);
		auto cond = result_as_basic_formula();
		result = std::make_unique<FutureFormula>(std::move(cond), formula.command);
	}

};

std::unique_ptr<Formula> plankton::copy(const Formula& formula) {
	CopyVisitor visitor;
	formula.accept(visitor);
	return std::move(visitor.result);
}

inline std::unique_ptr<Formula> mk_bool(bool value) {
	return std::make_unique<ExpressionFormula>(std::make_unique<BooleanValue>(value));
}

std::unique_ptr<Formula> plankton::make_true() {
	return mk_bool(true);
}

std::unique_ptr<Formula> plankton::make_false() {
	return mk_bool(false);
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

std::unique_ptr<Formula> plankton::unify(const Formula& /*formula*/, const Formula& /*other*/) {
	// creates a formula that implies the passed formulas
	throw std::logic_error("not yet implemented (plankton::unify)");
}

std::unique_ptr<Formula> plankton::unify(const std::vector<std::unique_ptr<Formula>>& /*formulas*/) {
	// creates a formula that implies the passed formulas
	throw std::logic_error("not yet implemented (plankton::unify)");
} 
