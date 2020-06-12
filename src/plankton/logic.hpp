#pragma once
#ifndef PLANKTON_LOGIC
#define PLANKTON_LOGIC


#include <vector>
#include <deque>
#include <memory>
#include <cassert>
#include <ostream>
#include <type_traits>
#include "cola/ast.hpp"
#include "plankton/flow.hpp"


namespace plankton {

	struct ConjunctionFormula;
	struct ImplicationFormula;
	struct ExpressionFormula;
	struct NegatedFormula;
	struct OwnershipFormula;
	struct LogicallyContainedFormula;
	struct FlowFormula;
	struct ObligationFormula;
	struct FulfillmentFormula;
	struct PastPredicate;
	struct FuturePredicate;
	struct Annotation;

	struct LogicVisitor {
		virtual void visit(const ConjunctionFormula& formula) = 0;
		virtual void visit(const ImplicationFormula& formula) = 0;
		virtual void visit(const ExpressionFormula& formula) = 0;
		virtual void visit(const NegatedFormula& formula) = 0;
		virtual void visit(const OwnershipFormula& formula) = 0;
		virtual void visit(const LogicallyContainedFormula& formula) = 0;
		virtual void visit(const FlowFormula& formula) = 0;
		virtual void visit(const ObligationFormula& formula) = 0;
		virtual void visit(const FulfillmentFormula& formula) = 0;
		virtual void visit(const PastPredicate& formula) = 0;
		virtual void visit(const FuturePredicate& formula) = 0;
		virtual void visit(const Annotation& formula) = 0;
	};

	struct LogicNonConstVisitor {
		virtual void visit(ConjunctionFormula& formula) = 0;
		virtual void visit(ImplicationFormula& formula) = 0;
		virtual void visit(ExpressionFormula& formula) = 0;
		virtual void visit(NegatedFormula& formula) = 0;
		virtual void visit(OwnershipFormula& formula) = 0;
		virtual void visit(LogicallyContainedFormula& formula) = 0;
		virtual void visit(FlowFormula& formula) = 0;
		virtual void visit(ObligationFormula& formula) = 0;
		virtual void visit(FulfillmentFormula& formula) = 0;
		virtual void visit(PastPredicate& formula) = 0;
		virtual void visit(FuturePredicate& formula) = 0;
		virtual void visit(Annotation& formula) = 0;
	};

	#define ACCEPT_FORMULA_VISITOR \
		virtual void accept(LogicNonConstVisitor& visitor) override { visitor.visit(*this); } \
		virtual void accept(LogicVisitor& visitor) const override { visitor.visit(*this); }

	struct Formula {
		Formula(const Formula& other) = delete;
		virtual ~Formula() = default;
		virtual void accept(LogicVisitor& visitor) const = 0;
		virtual void accept(LogicNonConstVisitor& visitor) = 0;
		protected: Formula() {}
	};

	struct ConjunctionFormula : public Formula {
		std::deque<std::unique_ptr<Formula>> conjuncts; // TODO: use list (interference freedom checks merge conjunctions frequently)?
		ACCEPT_FORMULA_VISITOR
	};

	struct ImplicationFormula : public Formula {
		std::unique_ptr<Formula> premise;
		std::unique_ptr<Formula> conclusion;

		ImplicationFormula(std::unique_ptr<Formula> premise_, std::unique_ptr<Formula> conclusion_) : premise(std::move(premise_)), conclusion(std::move(conclusion_)) {
			assert(premise);
			assert(conclusion);
		}
		ACCEPT_FORMULA_VISITOR
	};

	struct ExpressionFormula : public Formula {
		std::unique_ptr<cola::Expression> expr;

		ExpressionFormula(std::unique_ptr<cola::Expression> expr_) : expr(std::move(expr_)) {
			assert(expr);
			assert(expr->type() == cola::Type::bool_type());
		}
		ACCEPT_FORMULA_VISITOR
	};

	struct NegatedFormula : Formula {
		std::unique_ptr<Formula> formula;

		NegatedFormula(std::unique_ptr<Formula> formula_) : formula(std::move(formula_)) {
			assert(formula);
		}
		ACCEPT_FORMULA_VISITOR
	};

	struct OwnershipFormula : public Formula {
		std::unique_ptr<cola::VariableExpression> expr;

		OwnershipFormula(std::unique_ptr<cola::VariableExpression> expr_) : expr(std::move(expr_)) {
			assert(expr);
			assert(!expr->decl.is_shared);
			assert(expr->decl.type.sort == cola::Sort::PTR);
		}
		ACCEPT_FORMULA_VISITOR
	};

	struct LogicallyContainedFormula : public Formula {
		std::unique_ptr<cola::Expression> expr;

		LogicallyContainedFormula(std::unique_ptr<cola::Expression> expr_) : expr(std::move(expr_)) {
			assert(expr);
			assert(expr->sort() == cola::Sort::DATA);
		}
		ACCEPT_FORMULA_VISITOR
	};

	struct FlowFormula : Formula {
		std::unique_ptr<cola::Expression> expr;
		FlowValue flow;

		FlowFormula(std::unique_ptr<cola::Expression> expr_, FlowValue flow_) : expr(std::move(expr_)), flow(flow_) {
			assert(expr);
			assert(expr->sort() == cola::Sort::PTR);
		}
		ACCEPT_FORMULA_VISITOR
	};

	struct ObligationFormula : Formula {
		// TODO: fill
		ACCEPT_FORMULA_VISITOR
	};

	struct FulfillmentFormula : Formula {
		// TODO: fill
		ACCEPT_FORMULA_VISITOR
	};


	struct TimePredicate {
		TimePredicate(const TimePredicate& other) = delete;
		virtual ~TimePredicate() = default;
		virtual void accept(LogicVisitor& visitor) const = 0;
		virtual void accept(LogicNonConstVisitor& visitor) = 0;
		protected: TimePredicate() {}
	};

	struct PastPredicate : public TimePredicate {
		std::unique_ptr<Formula> formula;
		
		PastPredicate(std::unique_ptr<Formula> formula_) : formula(std::move(formula_)) {
			assert(formula);
		}
		ACCEPT_FORMULA_VISITOR
	};

	struct FuturePredicate : public TimePredicate  {
		std::unique_ptr<Formula> pre;
		std::unique_ptr<cola::Assignment> command;
		std::unique_ptr<Formula> post;

		FuturePredicate(std::unique_ptr<Formula> pre_, std::unique_ptr<cola::Assignment> cmd_, std::unique_ptr<Formula> post_)
			: pre(std::move(pre_)), command(std::move(cmd_)), post(std::move(post_)) {
				assert(pre);
				assert(command);
				assert(post);
			}
			ACCEPT_FORMULA_VISITOR
	};


	struct Annotation {
		std::unique_ptr<Formula> now;
		std::deque<std::unique_ptr<TimePredicate>> time;

		Annotation() : now(std::make_unique<ConjunctionFormula>()) {}
		Annotation(std::unique_ptr<Formula> now_) : now(std::move(now_)) {
			assert(now);
		}
		void accept(LogicVisitor& visitor) const { visitor.visit(*this); }
		void accept(LogicNonConstVisitor& visitor) { visitor.visit(*this); }
	};


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