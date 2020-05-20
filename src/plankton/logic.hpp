#pragma once
#ifndef PLANKTON_LOGIC
#define PLANKTON_LOGIC


#include <memory>
#include <cassert>
#include "cola/ast.hpp"


namespace plankton {

	struct ConjunctionFormula;
	struct BasicConjunctionFormula;
	struct ExpressionFormula;
	struct HistoryFormula;
	struct FutureFormula;

	struct FormulaVisitor {
		virtual void visit(const ConjunctionFormula& formula) = 0;
		virtual void visit(const BasicConjunctionFormula& formula) = 0;
		virtual void visit(const ExpressionFormula& formula) = 0;
		virtual void visit(const HistoryFormula& formula) = 0;
		virtual void visit(const FutureFormula& formula) = 0;
	};

	struct FormulaNonConstVisitor {
		virtual void visit(ConjunctionFormula& formula) = 0;
		virtual void visit(BasicConjunctionFormula& formula) = 0;
		virtual void visit(ExpressionFormula& formula) = 0;
		virtual void visit(HistoryFormula& formula) = 0;
		virtual void visit(FutureFormula& formula) = 0;
	};

	struct Formula {
		Formula(const Formula& other) = delete;
		virtual ~Formula() = default;
		virtual void accept(FormulaVisitor& visitor) const = 0;
		virtual void accept(FormulaNonConstVisitor& visitor) = 0;
		protected: Formula() {}
	};

	struct ConjunctionFormula : public Formula {
		std::vector<std::unique_ptr<Formula>> conjuncts;
		virtual void accept(FormulaVisitor& visitor) const override { visitor.visit(*this); }
		virtual void accept(FormulaNonConstVisitor& visitor) override { visitor.visit(*this); }
	};

	struct BasicFormula : public Formula {
		protected: BasicFormula() {}
	};

	struct BasicConjunctionFormula : public BasicFormula {
		std::vector<std::unique_ptr<BasicFormula>> conjuncts;
		virtual void accept(FormulaVisitor& visitor) const override { visitor.visit(*this); }
		virtual void accept(FormulaNonConstVisitor& visitor) override { visitor.visit(*this); }
	};

	struct ExpressionFormula : public BasicFormula {
		std::unique_ptr<cola::Expression> expr;
		ExpressionFormula(std::unique_ptr<cola::Expression> expr_) : expr(std::move(expr_)) {
			assert(expr->type() == cola::Type::bool_type());
		}
		virtual void accept(FormulaVisitor& visitor) const override { visitor.visit(*this); }
		virtual void accept(FormulaNonConstVisitor& visitor) override { visitor.visit(*this); }
	};

	// TODO: implement flow formulas, key in/notin DS formulas --> should be BasicFormula subclasses

	struct HistoryFormula : public Formula {
		std::unique_ptr<BasicFormula> condition;
		std::unique_ptr<BasicFormula> expression;
		
		HistoryFormula(std::unique_ptr<BasicFormula> cond, std::unique_ptr<BasicFormula> expr) : condition(std::move(cond)), expression(std::move(expr)) {
			assert(condition);
			assert(expression);
		}
		virtual void accept(FormulaVisitor& visitor) const override { visitor.visit(*this); }
		virtual void accept(FormulaNonConstVisitor& visitor) override { visitor.visit(*this); }
	};

	struct FutureFormula : public Formula {
		std::unique_ptr<BasicFormula> condition;
		const cola::Command& command;
		
		FutureFormula(std::unique_ptr<BasicFormula> cond, const cola::Command& cmd) : condition(std::move(cond)), command(cmd) {
			assert(condition);
		}
		virtual void accept(FormulaVisitor& visitor) const override { visitor.visit(*this); }
		virtual void accept(FormulaNonConstVisitor& visitor) override { visitor.visit(*this); }
	};


	std::unique_ptr<Formula> make_true();
	std::unique_ptr<Formula> make_false();
	std::unique_ptr<Formula> copy(const Formula& formula);

	
	/** Returns true if 'formula ==> implied' is a tautology.
	  */
	bool implies(const Formula& formula, const cola::Expression& implied);

	/** Returns true if 'formula ==> implied' is a tautology.
	  */
	bool implies(const Formula& formula, const Formula& implied);

	/** Returns true if 'formula <==> implied' is a tautology.
	  */
	bool is_equal(const Formula& formula, const Formula& other);

	/** Returns a formula F such that the formulas 'formula ==> F' and 'other ==> F' are tautologies.
	  * Opts for the strongest such formula F.
	  */
	std::unique_ptr<Formula> unify(const Formula& formula, const Formula& other);

	/** Returns a formula F such that the formula 'H ==> F' is a tautology for all H in formulas.
	  * Opts for the strongest such formula F.
	  */
	std::unique_ptr<Formula> unify(const std::vector<std::unique_ptr<Formula>>& formulas);

} // namespace plankton

#endif