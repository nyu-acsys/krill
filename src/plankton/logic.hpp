#pragma once
#ifndef PLANKTON_LOGIC
#define PLANKTON_LOGIC


#include <deque>
#include <memory>
#include "cola/ast.hpp"
#include "cola/visitors.hpp"
#include "plankton/flow.hpp"


namespace plankton {

	//
	// Visitors
	//

	struct AxiomConjunctionFormula;
	struct ImplicationFormula;
	struct ConjunctionFormula;
	struct NegatedAxiom;
	struct ExpressionAxiom;
	struct OwnershipAxiom;
	struct LogicallyContainedAxiom;
	struct KeysetContainsAxiom;
	struct FlowAxiom;
	struct ObligationAxiom;
	struct FulfillmentAxiom;
	struct PastPredicate;
	struct FuturePredicate;
	struct Annotation;

	struct LogicVisitor {
		virtual void visit(const AxiomConjunctionFormula& formula) = 0;
		virtual void visit(const ImplicationFormula& formula) = 0;
		virtual void visit(const ConjunctionFormula& formula) = 0;
		virtual void visit(const NegatedAxiom& formula) = 0;
		virtual void visit(const ExpressionAxiom& formula) = 0;
		virtual void visit(const OwnershipAxiom& formula) = 0;
		virtual void visit(const LogicallyContainedAxiom& formula) = 0;
		virtual void visit(const KeysetContainsAxiom& formula) = 0;
		virtual void visit(const FlowAxiom& formula) = 0;
		virtual void visit(const ObligationAxiom& formula) = 0;
		virtual void visit(const FulfillmentAxiom& formula) = 0;
		virtual void visit(const PastPredicate& formula) = 0;
		virtual void visit(const FuturePredicate& formula) = 0;
		virtual void visit(const Annotation& formula) = 0;
	};

	struct LogicNonConstVisitor {
		virtual void visit(AxiomConjunctionFormula& formula) = 0;
		virtual void visit(ImplicationFormula& formula) = 0;
		virtual void visit(ConjunctionFormula& formula) = 0;
		virtual void visit(NegatedAxiom& formula) = 0;
		virtual void visit(ExpressionAxiom& formula) = 0;
		virtual void visit(OwnershipAxiom& formula) = 0;
		virtual void visit(LogicallyContainedAxiom& formula) = 0;
		virtual void visit(KeysetContainsAxiom& formula) = 0;
		virtual void visit(FlowAxiom& formula) = 0;
		virtual void visit(ObligationAxiom& formula) = 0;
		virtual void visit(FulfillmentAxiom& formula) = 0;
		virtual void visit(PastPredicate& formula) = 0;
		virtual void visit(FuturePredicate& formula) = 0;
		virtual void visit(Annotation& formula) = 0;
	};

	//
	// Compound Formulas
	//

	struct Formula {
		Formula(const Formula& other) = delete;
		virtual ~Formula() = default;
		virtual void accept(LogicVisitor& visitor) const = 0;
		virtual void accept(LogicNonConstVisitor& visitor) = 0;
		protected: Formula() {}
	};

	#define ACCEPT_FORMULA_VISITOR \
		virtual void accept(LogicNonConstVisitor& visitor) override { visitor.visit(*this); } \
		virtual void accept(LogicVisitor& visitor) const override { visitor.visit(*this); }

	//
	// Compound Formulas
	//

	struct SimpleFormula : public Formula {
	};

	struct Axiom : public SimpleFormula {
	};

	struct AxiomConjunctionFormula : public SimpleFormula {
		std::deque<std::unique_ptr<Axiom>> conjuncts;
		
		ACCEPT_FORMULA_VISITOR
	};

	struct ImplicationFormula : public SimpleFormula {
		std::unique_ptr<AxiomConjunctionFormula> premise;
		std::unique_ptr<AxiomConjunctionFormula> conclusion;

		ImplicationFormula();
		ImplicationFormula(std::unique_ptr<AxiomConjunctionFormula> premise, std::unique_ptr<AxiomConjunctionFormula> conclusion);
		ACCEPT_FORMULA_VISITOR
	};

	struct ConjunctionFormula : public Formula {
		std::deque<std::unique_ptr<Axiom>> axioms;
		std::deque<std::unique_ptr<ImplicationFormula>> implications;

		ACCEPT_FORMULA_VISITOR
	};

	//
	// Axioms
	//

	struct NegatedAxiom : Axiom {
		std::unique_ptr<Axiom> axiom;

		NegatedAxiom(std::unique_ptr<Axiom> axiom);
		ACCEPT_FORMULA_VISITOR
	};

	struct ExpressionAxiom : public Axiom {
		std::unique_ptr<cola::Expression> expr;

		ExpressionAxiom(std::unique_ptr<cola::Expression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct OwnershipAxiom : public Axiom {
		std::unique_ptr<cola::VariableExpression> expr;

		OwnershipAxiom(std::unique_ptr<cola::VariableExpression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct LogicallyContainedAxiom : public Axiom {
		std::unique_ptr<cola::Expression> expr;

		LogicallyContainedAxiom(std::unique_ptr<cola::Expression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct KeysetContainsAxiom : public Axiom {
		std::unique_ptr<cola::Expression> node;
		std::unique_ptr<cola::Expression> value;

		KeysetContainsAxiom(std::unique_ptr<cola::Expression> node, std::unique_ptr<cola::Expression> value);
		ACCEPT_FORMULA_VISITOR
	};

	struct FlowAxiom : public Axiom {
		std::unique_ptr<cola::Expression> expr;
		FlowValue flow;

		FlowAxiom(std::unique_ptr<cola::Expression> expr, FlowValue flow);
		ACCEPT_FORMULA_VISITOR
	};

	struct SpecificationAxiom : public Axiom {
		enum struct Kind { CONTAINS, INSERT, DELETE };
		Kind kind;
		std::unique_ptr<cola::VariableExpression> key;

		SpecificationAxiom(Kind kind, std::unique_ptr<cola::VariableExpression> key);
	};

	struct ObligationAxiom : public SpecificationAxiom {
		ObligationAxiom(Kind kind, std::unique_ptr<cola::VariableExpression> key);
		ACCEPT_FORMULA_VISITOR
	};

	struct FulfillmentAxiom : public SpecificationAxiom {
		bool return_value;

		FulfillmentAxiom(Kind kind, std::unique_ptr<cola::VariableExpression> key, bool return_value);
		ACCEPT_FORMULA_VISITOR
	};

	//
	// Predicates
	//

	struct TimePredicate : public Formula {
	};

	struct PastPredicate : public TimePredicate {
		std::unique_ptr<ConjunctionFormula> formula;

		PastPredicate(std::unique_ptr<ConjunctionFormula> formula);
		ACCEPT_FORMULA_VISITOR
	};

	struct FuturePredicate : public TimePredicate  {
		std::unique_ptr<ConjunctionFormula> pre;
		std::unique_ptr<cola::Assignment> command;
		std::unique_ptr<ConjunctionFormula> post;

		FuturePredicate(std::unique_ptr<ConjunctionFormula> pre, std::unique_ptr<cola::Assignment> cmd, std::unique_ptr<ConjunctionFormula> post);
		ACCEPT_FORMULA_VISITOR
	};

	//
	// Annotation
	//

	struct Annotation : public Formula {
		std::unique_ptr<ConjunctionFormula> now;
		std::deque<std::unique_ptr<TimePredicate>> time;

		Annotation();
		Annotation(std::unique_ptr<ConjunctionFormula> now);
		static std::unique_ptr<Annotation> make_true();
		static std::unique_ptr<Annotation> make_false();
		ACCEPT_FORMULA_VISITOR
	};

	//
	// More Visitors
	//

	struct BaseLogicVisitor : public LogicVisitor {
		virtual void visit(const AxiomConjunctionFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const AxiomConjunctionFormula&"); }
		virtual void visit(const ImplicationFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ImplicationFormula&"); }
		virtual void visit(const ConjunctionFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ConjunctionFormula&"); }
		virtual void visit(const NegatedAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const NegatedAxiom&"); }
		virtual void visit(const ExpressionAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ExpressionAxiom&"); }
		virtual void visit(const OwnershipAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const OwnershipAxiom&"); }
		virtual void visit(const LogicallyContainedAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const LogicallyContainedAxiom&"); }
		virtual void visit(const KeysetContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const KeysetContainsAxiom&"); }
		virtual void visit(const FlowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FlowAxiom&"); }
		virtual void visit(const ObligationAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ObligationAxiom&"); }
		virtual void visit(const FulfillmentAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FulfillmentAxiom&"); }
		virtual void visit(const PastPredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const PastPredicate&"); }
		virtual void visit(const FuturePredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FuturePredicate&"); }
		virtual void visit(const Annotation& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const Annotation&"); }
	};

	struct BaseLogicNonConstVisitor : public LogicNonConstVisitor {
		virtual void visit(AxiomConjunctionFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(AxiomConjunctionFormula&"); }
		virtual void visit(ImplicationFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(ImplicationFormula&"); }
		virtual void visit(ConjunctionFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(ConjunctionFormula&"); }
		virtual void visit(NegatedAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(NegatedAxiom&"); }
		virtual void visit(ExpressionAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(ExpressionAxiom&"); }
		virtual void visit(OwnershipAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(OwnershipAxiom&"); }
		virtual void visit(LogicallyContainedAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(LogicallyContainedAxiom&"); }
		virtual void visit(KeysetContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(KeysetContainsAxiom&"); }
		virtual void visit(FlowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FlowAxiom&"); }
		virtual void visit(ObligationAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(ObligationAxiom&"); }
		virtual void visit(FulfillmentAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FulfillmentAxiom&"); }
		virtual void visit(PastPredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(PastPredicate&"); }
		virtual void visit(FuturePredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FuturePredicate&"); }
		virtual void visit(Annotation& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(Annotation&"); }
	};

	//
	// Listeners
	//

	struct LogicListener : public LogicVisitor {
		// compound elements
		template<typename T>
		void visit_all(const T& container) {
			for (const auto& elem : container) {
				elem->accept(*this);
			}
		}
		void visit(const ConjunctionFormula& formula) override {
			enter(formula);
			visit_all(formula.axioms);
			visit_all(formula.implications);
			exit(formula);
		}
		void visit(const AxiomConjunctionFormula& formula) override {
			enter(formula);
			visit_all(formula.conjuncts);
			exit(formula);
		}
		void visit(const ImplicationFormula& formula) override {
			enter(formula);
			formula.premise->accept(*this);
			formula.conclusion->accept(*this);
			exit(formula);
		}
		void visit(const NegatedAxiom& formula) override {
			enter(formula);
			formula.axiom->accept(*this);
			exit(formula);
		}
		void visit(const PastPredicate& formula) override {
			enter(formula);
			formula.formula->accept(*this);
			exit(formula);
		}
		void visit(const FuturePredicate& formula) override {
			enter(formula);
			formula.pre->accept(*this);
			formula.post->accept(*this);
			exit(formula);
		}
		void visit(const Annotation& formula) override {
			enter(formula);
			formula.now->accept(*this);
			visit_all(formula.time);
			exit(formula);
		}

		// non-decomposable axioms
		void visit(const ExpressionAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const OwnershipAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const LogicallyContainedAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const KeysetContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const FlowAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const ObligationAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const FulfillmentAxiom& formula) override { enter(formula); exit(formula); }

		// callbacks
		virtual void enter(const AxiomConjunctionFormula& formula) = 0;
		virtual void enter(const ImplicationFormula& formula) = 0;
		virtual void enter(const ConjunctionFormula& formula) = 0;
		virtual void enter(const NegatedAxiom& formula) = 0;
		virtual void enter(const ExpressionAxiom& formula) = 0;
		virtual void enter(const OwnershipAxiom& formula) = 0;
		virtual void enter(const LogicallyContainedAxiom& formula) = 0;
		virtual void enter(const KeysetContainsAxiom& formula) = 0;
		virtual void enter(const FlowAxiom& formula) = 0;
		virtual void enter(const ObligationAxiom& formula) = 0;
		virtual void enter(const FulfillmentAxiom& formula) = 0;
		virtual void enter(const PastPredicate& formula) = 0;
		virtual void enter(const FuturePredicate& formula) = 0;
		virtual void enter(const Annotation& formula) = 0;

		virtual void exit(const AxiomConjunctionFormula& formula) = 0;
		virtual void exit(const ImplicationFormula& formula) = 0;
		virtual void exit(const ConjunctionFormula& formula) = 0;
		virtual void exit(const NegatedAxiom& formula) = 0;
		virtual void exit(const ExpressionAxiom& formula) = 0;
		virtual void exit(const OwnershipAxiom& formula) = 0;
		virtual void exit(const LogicallyContainedAxiom& formula) = 0;
		virtual void exit(const KeysetContainsAxiom& formula) = 0;
		virtual void exit(const FlowAxiom& formula) = 0;
		virtual void exit(const ObligationAxiom& formula) = 0;
		virtual void exit(const FulfillmentAxiom& formula) = 0;
		virtual void exit(const PastPredicate& formula) = 0;
		virtual void exit(const FuturePredicate& formula) = 0;
		virtual void exit(const Annotation& formula) = 0;
	};

	struct DefaultListener : public LogicListener {
		virtual void enter(const AxiomConjunctionFormula& /*formula*/) override {}
		virtual void enter(const ImplicationFormula& /*formula*/) override {}
		virtual void enter(const ConjunctionFormula& /*formula*/) override {}
		virtual void enter(const NegatedAxiom& /*formula*/) override {}
		virtual void enter(const ExpressionAxiom& /*formula*/) override {}
		virtual void enter(const OwnershipAxiom& /*formula*/) override {}
		virtual void enter(const LogicallyContainedAxiom& /*formula*/) override {}
		virtual void enter(const KeysetContainsAxiom& /*formula*/) override {}
		virtual void enter(const FlowAxiom& /*formula*/) override {}
		virtual void enter(const ObligationAxiom& /*formula*/) override {}
		virtual void enter(const FulfillmentAxiom& /*formula*/) override {}
		virtual void enter(const PastPredicate& /*formula*/) override {}
		virtual void enter(const FuturePredicate& /*formula*/) override {}
		virtual void enter(const Annotation& /*formula*/) override {}

		virtual void exit(const AxiomConjunctionFormula& /*formula*/) override {}
		virtual void exit(const ImplicationFormula& /*formula*/) override {}
		virtual void exit(const ConjunctionFormula& /*formula*/) override {}
		virtual void exit(const NegatedAxiom& /*formula*/) override {}
		virtual void exit(const ExpressionAxiom& /*formula*/) override {}
		virtual void exit(const OwnershipAxiom& /*formula*/) override {}
		virtual void exit(const LogicallyContainedAxiom& /*formula*/) override {}
		virtual void exit(const KeysetContainsAxiom& /*formula*/) override {}
		virtual void exit(const FlowAxiom& /*formula*/) override {}
		virtual void exit(const ObligationAxiom& /*formula*/) override {}
		virtual void exit(const FulfillmentAxiom& /*formula*/) override {}
		virtual void exit(const PastPredicate& /*formula*/) override {}
		virtual void exit(const FuturePredicate& /*formula*/) override {}
		virtual void exit(const Annotation& /*formula*/) override {}
	};

	struct LogicNonConstListener : public LogicNonConstVisitor {
		// compound elements
		template<typename T>
		void visit_all(T& container) {
			for (auto& elem : container) {
				elem->accept(*this);
			}
		}
		void visit(ConjunctionFormula& formula) override {
			enter(formula);
			visit_all(formula.axioms);
			visit_all(formula.implications);
			exit(formula);
		}
		void visit(AxiomConjunctionFormula& formula) override {
			enter(formula);
			visit_all(formula.conjuncts);
			exit(formula);
		}
		void visit(ImplicationFormula& formula) override {
			enter(formula);
			formula.premise->accept(*this);
			formula.conclusion->accept(*this);
			exit(formula);
		}
		void visit(NegatedAxiom& formula) override {
			enter(formula);
			formula.axiom->accept(*this);
			exit(formula);
		}
		void visit(PastPredicate& formula) override {
			enter(formula);
			formula.formula->accept(*this);
			exit(formula);
		}
		void visit(FuturePredicate& formula) override {
			enter(formula);
			formula.pre->accept(*this);
			formula.post->accept(*this);
			exit(formula);
		}
		void visit(Annotation& formula) override {
			enter(formula);
			formula.now->accept(*this);
			visit_all(formula.time);
			exit(formula);
		}

		// non-decomposable axioms
		void visit(ExpressionAxiom& formula) override { enter(formula); exit(formula); }
		void visit(OwnershipAxiom& formula) override { enter(formula); exit(formula); }
		void visit(LogicallyContainedAxiom& formula) override { enter(formula); exit(formula); }
		void visit(KeysetContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(FlowAxiom& formula) override { enter(formula); exit(formula); }
		void visit(ObligationAxiom& formula) override { enter(formula); exit(formula); }
		void visit(FulfillmentAxiom& formula) override { enter(formula); exit(formula); }

		// callbacks
		virtual void enter(AxiomConjunctionFormula& formula) = 0;
		virtual void enter(ImplicationFormula& formula) = 0;
		virtual void enter(ConjunctionFormula& formula) = 0;
		virtual void enter(NegatedAxiom& formula) = 0;
		virtual void enter(ExpressionAxiom& formula) = 0;
		virtual void enter(OwnershipAxiom& formula) = 0;
		virtual void enter(LogicallyContainedAxiom& formula) = 0;
		virtual void enter(KeysetContainsAxiom& formula) = 0;
		virtual void enter(FlowAxiom& formula) = 0;
		virtual void enter(ObligationAxiom& formula) = 0;
		virtual void enter(FulfillmentAxiom& formula) = 0;
		virtual void enter(PastPredicate& formula) = 0;
		virtual void enter(FuturePredicate& formula) = 0;
		virtual void enter(Annotation& formula) = 0;

		virtual void exit(AxiomConjunctionFormula& formula) = 0;
		virtual void exit(ImplicationFormula& formula) = 0;
		virtual void exit(ConjunctionFormula& formula) = 0;
		virtual void exit(NegatedAxiom& formula) = 0;
		virtual void exit(ExpressionAxiom& formula) = 0;
		virtual void exit(OwnershipAxiom& formula) = 0;
		virtual void exit(LogicallyContainedAxiom& formula) = 0;
		virtual void exit(KeysetContainsAxiom& formula) = 0;
		virtual void exit(FlowAxiom& formula) = 0;
		virtual void exit(ObligationAxiom& formula) = 0;
		virtual void exit(FulfillmentAxiom& formula) = 0;
		virtual void exit(PastPredicate& formula) = 0;
		virtual void exit(FuturePredicate& formula) = 0;
		virtual void exit(Annotation& formula) = 0;
	};

	struct DefaultNonConstListener : public LogicNonConstListener {
		virtual void enter(AxiomConjunctionFormula& /*formula*/) override {}
		virtual void enter(ImplicationFormula& /*formula*/) override {}
		virtual void enter(ConjunctionFormula& /*formula*/) override {}
		virtual void enter(NegatedAxiom& /*formula*/) override {}
		virtual void enter(ExpressionAxiom& /*formula*/) override {}
		virtual void enter(OwnershipAxiom& /*formula*/) override {}
		virtual void enter(LogicallyContainedAxiom& /*formula*/) override {}
		virtual void enter(KeysetContainsAxiom& /*formula*/) override {}
		virtual void enter(FlowAxiom& /*formula*/) override {}
		virtual void enter(ObligationAxiom& /*formula*/) override {}
		virtual void enter(FulfillmentAxiom& /*formula*/) override {}
		virtual void enter(PastPredicate& /*formula*/) override {}
		virtual void enter(FuturePredicate& /*formula*/) override {}
		virtual void enter(Annotation& /*formula*/) override {}

		virtual void exit(AxiomConjunctionFormula& /*formula*/) override {}
		virtual void exit(ImplicationFormula& /*formula*/) override {}
		virtual void exit(ConjunctionFormula& /*formula*/) override {}
		virtual void exit(NegatedAxiom& /*formula*/) override {}
		virtual void exit(ExpressionAxiom& /*formula*/) override {}
		virtual void exit(OwnershipAxiom& /*formula*/) override {}
		virtual void exit(LogicallyContainedAxiom& /*formula*/) override {}
		virtual void exit(KeysetContainsAxiom& /*formula*/) override {}
		virtual void exit(FlowAxiom& /*formula*/) override {}
		virtual void exit(ObligationAxiom& /*formula*/) override {}
		virtual void exit(FulfillmentAxiom& /*formula*/) override {}
		virtual void exit(PastPredicate& /*formula*/) override {}
		virtual void exit(FuturePredicate& /*formula*/) override {}
		virtual void exit(Annotation& /*formula*/) override {}
	};

} // namespace plankton

#endif