#pragma once
#ifndef HEAL_VISITORS
#define HEAL_VISITORS


#include "cola/visitors.hpp"


namespace heal {

	struct AxiomConjunctionFormula;
	struct ImplicationFormula;
	struct ConjunctionFormula;
	struct NegatedAxiom;
	struct ExpressionAxiom;
	struct OwnershipAxiom;
	struct DataStructureLogicallyContainsAxiom;
	struct NodeLogicallyContainsAxiom;
	struct KeysetContainsAxiom;
	struct HasFlowAxiom;
	struct FlowContainsAxiom;
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
		virtual void visit(const DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void visit(const NodeLogicallyContainsAxiom& formula) = 0;
		virtual void visit(const KeysetContainsAxiom& formula) = 0;
		virtual void visit(const HasFlowAxiom& formula) = 0;
		virtual void visit(const FlowContainsAxiom& formula) = 0;
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
		virtual void visit(DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void visit(NodeLogicallyContainsAxiom& formula) = 0;
		virtual void visit(KeysetContainsAxiom& formula) = 0;
		virtual void visit(HasFlowAxiom& formula) = 0;
		virtual void visit(FlowContainsAxiom& formula) = 0;
		virtual void visit(ObligationAxiom& formula) = 0;
		virtual void visit(FulfillmentAxiom& formula) = 0;
		virtual void visit(PastPredicate& formula) = 0;
		virtual void visit(FuturePredicate& formula) = 0;
		virtual void visit(Annotation& formula) = 0;
	};


	struct BaseLogicVisitor : public LogicVisitor {
		virtual void visit(const AxiomConjunctionFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const AxiomConjunctionFormula&"); }
		virtual void visit(const ImplicationFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ImplicationFormula&"); }
		virtual void visit(const ConjunctionFormula& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ConjunctionFormula&"); }
		virtual void visit(const NegatedAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const NegatedAxiom&"); }
		virtual void visit(const ExpressionAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ExpressionAxiom&"); }
		virtual void visit(const OwnershipAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const OwnershipAxiom&"); }
		virtual void visit(const DataStructureLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const DataStructureLogicallyContainsAxiom&"); }
		virtual void visit(const NodeLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const NodeLogicallyContainsAxiom&"); }
		virtual void visit(const KeysetContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const KeysetContainsAxiom&"); }
		virtual void visit(const HasFlowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const HasFlowAxiom&"); }
		virtual void visit(const FlowContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FlowContainsAxiom&"); }
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
		virtual void visit(DataStructureLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(DataStructureLogicallyContainsAxiom&"); }
		virtual void visit(NodeLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(NodeLogicallyContainsAxiom&"); }
		virtual void visit(KeysetContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(KeysetContainsAxiom&"); }
		virtual void visit(HasFlowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(HasFlowAxiom&"); }
		virtual void visit(FlowContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FlowContainsAxiom&"); }
		virtual void visit(ObligationAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(ObligationAxiom&"); }
		virtual void visit(FulfillmentAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FulfillmentAxiom&"); }
		virtual void visit(PastPredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(PastPredicate&"); }
		virtual void visit(FuturePredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FuturePredicate&"); }
		virtual void visit(Annotation& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(Annotation&"); }
	};


	struct LogicListener : public LogicVisitor {
		// compound elements
		void visit(const ConjunctionFormula& formula) override;
		void visit(const AxiomConjunctionFormula& formula) override;
		void visit(const ImplicationFormula& formula) override;
		void visit(const NegatedAxiom& formula) override;
		void visit(const PastPredicate& formula) override;
		void visit(const FuturePredicate& formula) override;
		void visit(const Annotation& formula) override;

		// non-decomposable axioms
		void visit(const ExpressionAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const OwnershipAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const DataStructureLogicallyContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const NodeLogicallyContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const KeysetContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const HasFlowAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const FlowContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const ObligationAxiom& formula) override { enter(formula); exit(formula); }
		void visit(const FulfillmentAxiom& formula) override { enter(formula); exit(formula); }

		// callbacks
		virtual void enter(const AxiomConjunctionFormula& formula) = 0;
		virtual void enter(const ImplicationFormula& formula) = 0;
		virtual void enter(const ConjunctionFormula& formula) = 0;
		virtual void enter(const NegatedAxiom& formula) = 0;
		virtual void enter(const ExpressionAxiom& formula) = 0;
		virtual void enter(const OwnershipAxiom& formula) = 0;
		virtual void enter(const DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void enter(const NodeLogicallyContainsAxiom& formula) = 0;
		virtual void enter(const KeysetContainsAxiom& formula) = 0;
		virtual void enter(const HasFlowAxiom& formula) = 0;
		virtual void enter(const FlowContainsAxiom& formula) = 0;
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
		virtual void exit(const DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void exit(const NodeLogicallyContainsAxiom& formula) = 0;
		virtual void exit(const KeysetContainsAxiom& formula) = 0;
		virtual void exit(const HasFlowAxiom& formula) = 0;
		virtual void exit(const FlowContainsAxiom& formula) = 0;
		virtual void exit(const ObligationAxiom& formula) = 0;
		virtual void exit(const FulfillmentAxiom& formula) = 0;
		virtual void exit(const PastPredicate& formula) = 0;
		virtual void exit(const FuturePredicate& formula) = 0;
		virtual void exit(const Annotation& formula) = 0;
	};

	struct LogicNonConstListener : public LogicNonConstVisitor {
		// compound elements
		void visit(ConjunctionFormula& formula) override;
		void visit(AxiomConjunctionFormula& formula) override;
		void visit(ImplicationFormula& formula) override;
		void visit(NegatedAxiom& formula) override;
		void visit(PastPredicate& formula) override;
		void visit(FuturePredicate& formula) override;
		void visit(Annotation& formula) override;

		// non-decomposable axioms
		void visit(ExpressionAxiom& formula) override { enter(formula); exit(formula); }
		void visit(OwnershipAxiom& formula) override { enter(formula); exit(formula); }
		void visit(DataStructureLogicallyContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(NodeLogicallyContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(KeysetContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(HasFlowAxiom& formula) override { enter(formula); exit(formula); }
		void visit(FlowContainsAxiom& formula) override { enter(formula); exit(formula); }
		void visit(ObligationAxiom& formula) override { enter(formula); exit(formula); }
		void visit(FulfillmentAxiom& formula) override { enter(formula); exit(formula); }

		// callbacks
		virtual void enter(AxiomConjunctionFormula& formula) = 0;
		virtual void enter(ImplicationFormula& formula) = 0;
		virtual void enter(ConjunctionFormula& formula) = 0;
		virtual void enter(NegatedAxiom& formula) = 0;
		virtual void enter(ExpressionAxiom& formula) = 0;
		virtual void enter(OwnershipAxiom& formula) = 0;
		virtual void enter(DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void enter(NodeLogicallyContainsAxiom& formula) = 0;
		virtual void enter(KeysetContainsAxiom& formula) = 0;
		virtual void enter(HasFlowAxiom& formula) = 0;
		virtual void enter(FlowContainsAxiom& formula) = 0;
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
		virtual void exit(DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void exit(NodeLogicallyContainsAxiom& formula) = 0;
		virtual void exit(KeysetContainsAxiom& formula) = 0;
		virtual void exit(HasFlowAxiom& formula) = 0;
		virtual void exit(FlowContainsAxiom& formula) = 0;
		virtual void exit(ObligationAxiom& formula) = 0;
		virtual void exit(FulfillmentAxiom& formula) = 0;
		virtual void exit(PastPredicate& formula) = 0;
		virtual void exit(FuturePredicate& formula) = 0;
		virtual void exit(Annotation& formula) = 0;
	};


	struct DefaultListener : public LogicListener {
		virtual void enter(const AxiomConjunctionFormula& /*formula*/) override {}
		virtual void enter(const ImplicationFormula& /*formula*/) override {}
		virtual void enter(const ConjunctionFormula& /*formula*/) override {}
		virtual void enter(const NegatedAxiom& /*formula*/) override {}
		virtual void enter(const ExpressionAxiom& /*formula*/) override {}
		virtual void enter(const OwnershipAxiom& /*formula*/) override {}
		virtual void enter(const DataStructureLogicallyContainsAxiom& /*formula*/) override {}
		virtual void enter(const NodeLogicallyContainsAxiom& /*formula*/) override {}
		virtual void enter(const KeysetContainsAxiom& /*formula*/) override {}
		virtual void enter(const HasFlowAxiom& /*formula*/) override {}
		virtual void enter(const FlowContainsAxiom& /*formula*/) override {}
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
		virtual void exit(const DataStructureLogicallyContainsAxiom& /*formula*/) override {}
		virtual void exit(const NodeLogicallyContainsAxiom& /*formula*/) override {}
		virtual void exit(const KeysetContainsAxiom& /*formula*/) override {}
		virtual void exit(const HasFlowAxiom& /*formula*/) override {}
		virtual void exit(const FlowContainsAxiom& /*formula*/) override {}
		virtual void exit(const ObligationAxiom& /*formula*/) override {}
		virtual void exit(const FulfillmentAxiom& /*formula*/) override {}
		virtual void exit(const PastPredicate& /*formula*/) override {}
		virtual void exit(const FuturePredicate& /*formula*/) override {}
		virtual void exit(const Annotation& /*formula*/) override {}
	};

	struct DefaultNonConstListener : public LogicNonConstListener {
		virtual void enter(AxiomConjunctionFormula& /*formula*/) override {}
		virtual void enter(ImplicationFormula& /*formula*/) override {}
		virtual void enter(ConjunctionFormula& /*formula*/) override {}
		virtual void enter(NegatedAxiom& /*formula*/) override {}
		virtual void enter(ExpressionAxiom& /*formula*/) override {}
		virtual void enter(OwnershipAxiom& /*formula*/) override {}
		virtual void enter(DataStructureLogicallyContainsAxiom& /*formula*/) override {}
		virtual void enter(NodeLogicallyContainsAxiom& /*formula*/) override {}
		virtual void enter(KeysetContainsAxiom& /*formula*/) override {}
		virtual void enter(HasFlowAxiom& /*formula*/) override {}
		virtual void enter(FlowContainsAxiom& /*formula*/) override {}
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
		virtual void exit(DataStructureLogicallyContainsAxiom& /*formula*/) override {}
		virtual void exit(NodeLogicallyContainsAxiom& /*formula*/) override {}
		virtual void exit(KeysetContainsAxiom& /*formula*/) override {}
		virtual void exit(HasFlowAxiom& /*formula*/) override {}
		virtual void exit(FlowContainsAxiom& /*formula*/) override {}
		virtual void exit(ObligationAxiom& /*formula*/) override {}
		virtual void exit(FulfillmentAxiom& /*formula*/) override {}
		virtual void exit(PastPredicate& /*formula*/) override {}
		virtual void exit(FuturePredicate& /*formula*/) override {}
		virtual void exit(Annotation& /*formula*/) override {}
	};

} // namespace heal

#endif