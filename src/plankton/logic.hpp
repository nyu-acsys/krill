#pragma once
#ifndef PLANKTON_LOGIC
#define PLANKTON_LOGIC


#include <memory>
#include <cassert>
#include "cola/ast.hpp"


namespace plankton {

	struct ConjunctionFormula;
	struct ExpressionFormula;
	struct PastPredicate;
	struct FuturePredicate;
	struct Annotation;

	struct LogicVisitor {
		virtual void visit(const ConjunctionFormula& formula) = 0;
		virtual void visit(const ExpressionFormula& formula) = 0;
		virtual void visit(const PastPredicate& formula) = 0;
		virtual void visit(const FuturePredicate& formula) = 0;
		virtual void visit(const Annotation& formula) = 0;
	};

	struct LogicNonConstVisitor {
		virtual void visit(ConjunctionFormula& formula) = 0;
		virtual void visit(ExpressionFormula& formula) = 0;
		virtual void visit(PastPredicate& formula) = 0;
		virtual void visit(FuturePredicate& formula) = 0;
		virtual void visit(Annotation& formula) = 0;
	};

	struct Formula {
		Formula(const Formula& other) = delete;
		virtual ~Formula() = default;
		virtual void accept(LogicVisitor& visitor) const = 0;
		virtual void accept(LogicNonConstVisitor& visitor) = 0;
		protected: Formula() {}
	};

	struct ConjunctionFormula : public Formula {
		std::vector<std::unique_ptr<Formula>> conjuncts;
		virtual void accept(LogicVisitor& visitor) const override { visitor.visit(*this); }
		virtual void accept(LogicNonConstVisitor& visitor) override { visitor.visit(*this); }
	};

	struct ExpressionFormula : public Formula {
		std::unique_ptr<cola::Expression> expr;
		ExpressionFormula(std::unique_ptr<cola::Expression> expr_) : expr(std::move(expr_)) {
			assert(expr->type() == cola::Type::bool_type());
		}
		virtual void accept(LogicVisitor& visitor) const override { visitor.visit(*this); }
		virtual void accept(LogicNonConstVisitor& visitor) override { visitor.visit(*this); }
	};

	// TODO: implement flow formulas, key in/notin DS formulas

	struct PastPredicate {
		std::unique_ptr<Formula> formula;
		
		PastPredicate(std::unique_ptr<Formula> formula_) : formula(std::move(formula_)) {
			assert(formula);
		}
		void accept(LogicVisitor& visitor) const { visitor.visit(*this); }
		void accept(LogicNonConstVisitor& visitor) { visitor.visit(*this); }
	};

	struct FuturePredicate {
		std::unique_ptr<Formula> pre;
		std::unique_ptr<cola::Command> command;
		std::unique_ptr<Formula> post;

		FuturePredicate(std::unique_ptr<Formula> pre_, std::unique_ptr<cola::Command> cmd_, std::unique_ptr<Formula> post_)
			: pre(std::move(pre_)), command(std::move(cmd_)), post(std::move(post_)) {
				assert(pre);
				assert(command);
				assert(post);
			}
		void accept(LogicVisitor& visitor) const { visitor.visit(*this); }
		void accept(LogicNonConstVisitor& visitor) { visitor.visit(*this); }
	};

	struct Annotation {
		std::unique_ptr<ConjunctionFormula> now;
		std::vector<PastPredicate> past;
		std::vector<FuturePredicate> future;

		Annotation(std::unique_ptr<ConjunctionFormula> now_) : now(std::move(now_)) {
			assert(now);
		}
		Annotation() : now(std::make_unique<ConjunctionFormula>()) {}
		void accept(LogicVisitor& visitor) const { visitor.visit(*this); }
		void accept(LogicNonConstVisitor& visitor) { visitor.visit(*this); }
	};


	std::unique_ptr<Formula> copy(const Formula& formula);
	PastPredicate copy(const PastPredicate& predicate);
	FuturePredicate copy(const FuturePredicate& predicate);
	std::unique_ptr<Annotation> copy(const Annotation& annotation);


	std::unique_ptr<Annotation> make_true();
	std::unique_ptr<Annotation> make_false();

	
	/** Returns true if 'formula ==> (expr != NULL)' is a tautology.
	  */
	bool implies_nonnull(const Formula& formula, const cola::Expression& expr);
	
	/** Returns true if 'formula ==> condition' is a tautology.
	  */
	bool implies(const Formula& formula, const cola::Expression& condition);

	/** Returns true if 'formula ==> implied' is a tautology.
	  */
	bool implies(const Formula& formula, const Formula& implied);

	/** Returns true if 'formula <==> implied' is a tautology.
	  */
	bool is_equal(const Annotation& annotation, const Annotation& other);

	/** Returns a formula F such that the formulas 'formula ==> F' and 'other ==> F' are tautologies.
	  * Opts for the strongest such formula F.
	  */
	std::unique_ptr<Annotation> unify(const Annotation& annotation, const Annotation& other);

	/** Returns a formula F such that the formula 'H ==> F' is a tautology for all H in formulas.
	  * Opts for the strongest such formula F.
	  */
	std::unique_ptr<Annotation> unify(const std::vector<std::unique_ptr<Annotation>>& annotations);

} // namespace plankton

#endif