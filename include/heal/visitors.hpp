#pragma once
#ifndef HEAL_VISITORS
#define HEAL_VISITORS


#include "cola/visitors.hpp"


namespace heal {
    struct LogicVariable;
    struct SymbolicBool;
    struct SymbolicNull;
    struct SymbolicMin;
    struct SymbolicMax;
    struct FlatSeparatingConjunction;
    struct SeparatingConjunction;
    struct SeparatingImplication;
    struct NegatedAxiom;
    struct BoolAxiom;
    struct PointsToAxiom;
    struct StackAxiom;
    struct StackDisjunction;
    struct OwnershipAxiom;
    struct DataStructureLogicallyContainsAxiom;
    struct NodeLogicallyContainsAxiom;
    struct KeysetContainsAxiom;
    struct HasFlowAxiom;
    struct UniqueInflowAxiom;
    struct FlowContainsAxiom;
    struct ObligationAxiom;
    struct FulfillmentAxiom;
	struct PastPredicate;
	struct FuturePredicate;
	struct Annotation;


	struct LogicVisitor {
        virtual void visit(const LogicVariable& expression) = 0;
        virtual void visit(const SymbolicBool& expression) = 0;
        virtual void visit(const SymbolicNull& expression) = 0;
        virtual void visit(const SymbolicMin& expression) = 0;
        virtual void visit(const SymbolicMax& expression) = 0;
		virtual void visit(const FlatSeparatingConjunction& formula) = 0;
        virtual void visit(const SeparatingConjunction& formula) = 0;
        virtual void visit(const SeparatingImplication& formula) = 0;
        virtual void visit(const NegatedAxiom& formula) = 0;
        virtual void visit(const BoolAxiom& formula) = 0;
        virtual void visit(const PointsToAxiom& formula) = 0;
        virtual void visit(const StackAxiom& formula) = 0;
        virtual void visit(const StackDisjunction& formula) = 0;
		virtual void visit(const OwnershipAxiom& formula) = 0;
		virtual void visit(const DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void visit(const NodeLogicallyContainsAxiom& formula) = 0;
		virtual void visit(const KeysetContainsAxiom& formula) = 0;
		virtual void visit(const HasFlowAxiom& formula) = 0;
		virtual void visit(const FlowContainsAxiom& formula) = 0;
		virtual void visit(const UniqueInflowAxiom& formula) = 0;
		virtual void visit(const ObligationAxiom& formula) = 0;
		virtual void visit(const FulfillmentAxiom& formula) = 0;
		virtual void visit(const PastPredicate& formula) = 0;
		virtual void visit(const FuturePredicate& formula) = 0;
		virtual void visit(const Annotation& formula) = 0;
	};

	struct LogicNonConstVisitor {
        virtual void visit(LogicVariable& expression) = 0;
        virtual void visit(SymbolicBool& expression) = 0;
        virtual void visit(SymbolicNull& expression) = 0;
        virtual void visit(SymbolicMin& expression) = 0;
        virtual void visit(SymbolicMax& expression) = 0;
        virtual void visit(FlatSeparatingConjunction& formula) = 0;
        virtual void visit(SeparatingConjunction& formula) = 0;
        virtual void visit(SeparatingImplication& formula) = 0;
        virtual void visit(NegatedAxiom& formula) = 0;
        virtual void visit(BoolAxiom& formula) = 0;
        virtual void visit(PointsToAxiom& formula) = 0;
        virtual void visit(StackAxiom& formula) = 0;
        virtual void visit(StackDisjunction& formula) = 0;
		virtual void visit(OwnershipAxiom& formula) = 0;
		virtual void visit(DataStructureLogicallyContainsAxiom& formula) = 0;
		virtual void visit(NodeLogicallyContainsAxiom& formula) = 0;
		virtual void visit(KeysetContainsAxiom& formula) = 0;
		virtual void visit(HasFlowAxiom& formula) = 0;
		virtual void visit(FlowContainsAxiom& formula) = 0;
		virtual void visit(UniqueInflowAxiom& formula) = 0;
		virtual void visit(ObligationAxiom& formula) = 0;
		virtual void visit(FulfillmentAxiom& formula) = 0;
		virtual void visit(PastPredicate& formula) = 0;
		virtual void visit(FuturePredicate& formula) = 0;
		virtual void visit(Annotation& formula) = 0;
	};


	struct BaseLogicVisitor : public LogicVisitor {
        void visit(const LogicVariable& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const LogicVariable&"); }
        void visit(const SymbolicBool& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicBool&"); }
        void visit(const SymbolicNull& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicNull&"); }
        void visit(const SymbolicMin& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicMin&"); }
        void visit(const SymbolicMax& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicMax&"); }
        void visit(const FlatSeparatingConjunction& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FlatSeparatingConjunction&"); }
        void visit(const SeparatingConjunction& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SeparatingConjunction&"); }
        void visit(const SeparatingImplication& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SeparatingImplication&"); }
        void visit(const NegatedAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const NegatedAxiom&"); }
        void visit(const BoolAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const BoolAxiom&"); }
        void visit(const PointsToAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const PointsToAxiom&"); }
        void visit(const StackAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const StackAxiom&"); }
        void visit(const StackDisjunction& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const StackDisjunction&"); }
		void visit(const OwnershipAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const OwnershipAxiom&"); }
		void visit(const DataStructureLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const DataStructureLogicallyContainsAxiom&"); }
		void visit(const NodeLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const NodeLogicallyContainsAxiom&"); }
		void visit(const KeysetContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const KeysetContainsAxiom&"); }
		void visit(const HasFlowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const HasFlowAxiom&"); }
		void visit(const FlowContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FlowContainsAxiom&"); }
		void visit(const UniqueInflowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const UniqueInflowAxiom&"); }
		void visit(const ObligationAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ObligationAxiom&"); }
		void visit(const FulfillmentAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FulfillmentAxiom&"); }
		void visit(const PastPredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const PastPredicate&"); }
		void visit(const FuturePredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FuturePredicate&"); }
		void visit(const Annotation& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const Annotation&"); }
	};

	struct BaseLogicNonConstVisitor : public LogicNonConstVisitor {
        void visit(LogicVariable& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(ProgramVariable&"); }
        void visit(SymbolicBool& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(SymbolicBool&"); }
        void visit(SymbolicNull& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(SymbolicNull&"); }
        void visit(SymbolicMin& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(SymbolicMin&"); }
        void visit(SymbolicMax& /*expression*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(SymbolicMax&"); }
        void visit(FlatSeparatingConjunction& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FlatSeparatingConjunction&"); }
        void visit(SeparatingConjunction& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(SeparatingConjunction&"); }
        void visit(SeparatingImplication& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(SeparatingImplication&"); }
        void visit(NegatedAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(NegatedAxiom&"); }
        void visit(BoolAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(BoolAxiom&"); }
        void visit(PointsToAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(PointsToAxiom&"); }
        void visit(StackAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(StackAxiom&"); }
        void visit(StackDisjunction& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(StackDisjunction&"); }
		void visit(OwnershipAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(OwnershipAxiom&"); }
		void visit(DataStructureLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(DataStructureLogicallyContainsAxiom&"); }
		void visit(NodeLogicallyContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(NodeLogicallyContainsAxiom&"); }
		void visit(KeysetContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(KeysetContainsAxiom&"); }
		void visit(HasFlowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(HasFlowAxiom&"); }
		void visit(FlowContainsAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FlowContainsAxiom&"); }
		void visit(UniqueInflowAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(UniqueInflowAxiom&"); }
		void visit(ObligationAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(ObligationAxiom&"); }
		void visit(FulfillmentAxiom& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FulfillmentAxiom&"); }
		void visit(PastPredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(PastPredicate&"); }
		void visit(FuturePredicate& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(FuturePredicate&"); }
		void visit(Annotation& /*formula*/) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "visit(Annotation&"); }
	};


	struct LogicListener : public LogicVisitor {
		// visit
        void visit(const LogicVariable& expression) override;
        void visit(const SymbolicBool& expression) override;
        void visit(const SymbolicNull& expression) override;
        void visit(const SymbolicMin& expression) override;
        void visit(const SymbolicMax& expression) override;
        void visit(const FlatSeparatingConjunction& formula) override;
        void visit(const SeparatingConjunction& formula) override;
        void visit(const SeparatingImplication& formula) override;
        void visit(const NegatedAxiom& formula) override;
        void visit(const BoolAxiom& formula) override;
        void visit(const PointsToAxiom& formula) override;
        void visit(const StackAxiom& formula) override;
        void visit(const StackDisjunction& formula) override;
		void visit(const OwnershipAxiom& formula) override;
		void visit(const DataStructureLogicallyContainsAxiom& formula) override;
		void visit(const NodeLogicallyContainsAxiom& formula) override;
		void visit(const KeysetContainsAxiom& formula) override;
		void visit(const HasFlowAxiom& formula) override;
		void visit(const FlowContainsAxiom& formula) override;
		void visit(const UniqueInflowAxiom& formula) override;
		void visit(const ObligationAxiom& formula) override;
		void visit(const FulfillmentAxiom& formula) override;
        void visit(const PastPredicate& formula) override;
        void visit(const FuturePredicate& formula) override;
        void visit(const Annotation& formula) override;

		// callbacks
        virtual void enter(const LogicVariable& decl) = 0;
        virtual void enter(const SymbolicBool& expression) = 0;
        virtual void enter(const SymbolicNull& expression) = 0;
        virtual void enter(const SymbolicMin& expression) = 0;
        virtual void enter(const SymbolicMax& expression) = 0;
        virtual void enter(const FlatSeparatingConjunction& formula) = 0;
        virtual void enter(const SeparatingConjunction& formula) = 0;
        virtual void enter(const SeparatingImplication& formula) = 0;
        virtual void enter(const NegatedAxiom& formula) = 0;
        virtual void enter(const BoolAxiom& formula) = 0;
        virtual void enter(const PointsToAxiom& formula) = 0;
        virtual void enter(const StackAxiom& formula) = 0;
        virtual void enter(const StackDisjunction& formula) = 0;
        virtual void enter(const OwnershipAxiom& formula) = 0;
        virtual void enter(const DataStructureLogicallyContainsAxiom& formula) = 0;
        virtual void enter(const NodeLogicallyContainsAxiom& formula) = 0;
        virtual void enter(const KeysetContainsAxiom& formula) = 0;
        virtual void enter(const HasFlowAxiom& formula) = 0;
        virtual void enter(const FlowContainsAxiom& formula) = 0;
        virtual void enter(const UniqueInflowAxiom& formula) = 0;
        virtual void enter(const ObligationAxiom& formula) = 0;
        virtual void enter(const FulfillmentAxiom& formula) = 0;
        virtual void enter(const PastPredicate& formula) = 0;
        virtual void enter(const FuturePredicate& formula) = 0;
        virtual void enter(const Annotation& formula) = 0;

        virtual void exit(const LogicVariable& decl) = 0;
        virtual void exit(const SymbolicBool& expression) = 0;
        virtual void exit(const SymbolicNull& expression) = 0;
        virtual void exit(const SymbolicMin& expression) = 0;
        virtual void exit(const SymbolicMax& expression) = 0;
        virtual void exit(const FlatSeparatingConjunction& formula) = 0;
        virtual void exit(const SeparatingConjunction& formula) = 0;
        virtual void exit(const SeparatingImplication& formula) = 0;
        virtual void exit(const NegatedAxiom& formula) = 0;
        virtual void exit(const BoolAxiom& formula) = 0;
        virtual void exit(const PointsToAxiom& formula) = 0;
        virtual void exit(const StackAxiom& formula) = 0;
        virtual void exit(const StackDisjunction& formula) = 0;
        virtual void exit(const OwnershipAxiom& formula) = 0;
        virtual void exit(const DataStructureLogicallyContainsAxiom& formula) = 0;
        virtual void exit(const NodeLogicallyContainsAxiom& formula) = 0;
        virtual void exit(const KeysetContainsAxiom& formula) = 0;
        virtual void exit(const HasFlowAxiom& formula) = 0;
        virtual void exit(const FlowContainsAxiom& formula) = 0;
        virtual void exit(const UniqueInflowAxiom& formula) = 0;
        virtual void exit(const ObligationAxiom& formula) = 0;
        virtual void exit(const FulfillmentAxiom& formula) = 0;
        virtual void exit(const PastPredicate& formula) = 0;
        virtual void exit(const FuturePredicate& formula) = 0;
        virtual void exit(const Annotation& formula) = 0;
	};

	struct LogicNonConstListener : public LogicNonConstVisitor {
		// visit
        void visit(LogicVariable& expression) override;
        void visit(SymbolicBool& expression) override;
        void visit(SymbolicNull& expression) override;
        void visit(SymbolicMin& expression) override;
        void visit(SymbolicMax& expression) override;
        void visit(FlatSeparatingConjunction& formula) override;
        void visit(SeparatingConjunction& formula) override;
        void visit(SeparatingImplication& formula) override;
        void visit(NegatedAxiom& formula) override;
        void visit(BoolAxiom& formula) override;
        void visit(PointsToAxiom& formula) override;
        void visit(StackAxiom& formula) override;
        void visit(StackDisjunction& formula) override;
        void visit(OwnershipAxiom& formula) override;
        void visit(DataStructureLogicallyContainsAxiom& formula) override;
        void visit(NodeLogicallyContainsAxiom& formula) override;
        void visit(KeysetContainsAxiom& formula) override;
        void visit(HasFlowAxiom& formula) override;
        void visit(FlowContainsAxiom& formula) override;
        void visit(UniqueInflowAxiom& formula) override;
        void visit(ObligationAxiom& formula) override;
        void visit(FulfillmentAxiom& formula) override;
        void visit(PastPredicate& formula) override;
        void visit(FuturePredicate& formula) override;
        void visit(Annotation& formula) override;

		// callbacks
        virtual void enter(LogicVariable& decl) = 0;
        virtual void enter(SymbolicBool& expression) = 0;
        virtual void enter(SymbolicNull& expression) = 0;
        virtual void enter(SymbolicMin& expression) = 0;
        virtual void enter(SymbolicMax& expression) = 0;
        virtual void enter(FlatSeparatingConjunction& formula) = 0;
        virtual void enter(SeparatingConjunction& formula) = 0;
        virtual void enter(SeparatingImplication& formula) = 0;
        virtual void enter(NegatedAxiom& formula) = 0;
        virtual void enter(BoolAxiom& formula) = 0;
        virtual void enter(PointsToAxiom& formula) = 0;
        virtual void enter(StackAxiom& formula) = 0;
        virtual void enter(StackDisjunction& formula) = 0;
        virtual void enter(OwnershipAxiom& formula) = 0;
        virtual void enter(DataStructureLogicallyContainsAxiom& formula) = 0;
        virtual void enter(NodeLogicallyContainsAxiom& formula) = 0;
        virtual void enter(KeysetContainsAxiom& formula) = 0;
        virtual void enter(HasFlowAxiom& formula) = 0;
        virtual void enter(FlowContainsAxiom& formula) = 0;
        virtual void enter(UniqueInflowAxiom& formula) = 0;
        virtual void enter(ObligationAxiom& formula) = 0;
        virtual void enter(FulfillmentAxiom& formula) = 0;
        virtual void enter(PastPredicate& formula) = 0;
        virtual void enter(FuturePredicate& formula) = 0;
        virtual void enter(Annotation& formula) = 0;

        virtual void exit(LogicVariable& decl) = 0;
        virtual void exit(SymbolicBool& expression) = 0;
        virtual void exit(SymbolicNull& expression) = 0;
        virtual void exit(SymbolicMin& expression) = 0;
        virtual void exit(SymbolicMax& expression) = 0;
        virtual void exit(FlatSeparatingConjunction& formula) = 0;
        virtual void exit(SeparatingConjunction& formula) = 0;
        virtual void exit(SeparatingImplication& formula) = 0;
        virtual void exit(NegatedAxiom& formula) = 0;
        virtual void exit(BoolAxiom& formula) = 0;
        virtual void exit(PointsToAxiom& formula) = 0;
        virtual void exit(StackAxiom& formula) = 0;
        virtual void exit(StackDisjunction& formula) = 0;
        virtual void exit(OwnershipAxiom& formula) = 0;
        virtual void exit(DataStructureLogicallyContainsAxiom& formula) = 0;
        virtual void exit(NodeLogicallyContainsAxiom& formula) = 0;
        virtual void exit(KeysetContainsAxiom& formula) = 0;
        virtual void exit(HasFlowAxiom& formula) = 0;
        virtual void exit(FlowContainsAxiom& formula) = 0;
        virtual void exit(UniqueInflowAxiom& formula) = 0;
        virtual void exit(ObligationAxiom& formula) = 0;
        virtual void exit(FulfillmentAxiom& formula) = 0;
        virtual void exit(PastPredicate& formula) = 0;
        virtual void exit(FuturePredicate& formula) = 0;
        virtual void exit(Annotation& formula) = 0;
	};


	struct DefaultLogicListener : public LogicListener {
        void enter(const LogicVariable&) override {}
        void enter(const SymbolicBool&) override {}
        void enter(const SymbolicNull&) override {}
        void enter(const SymbolicMin&) override {}
        void enter(const SymbolicMax&) override {}
        void enter(const FlatSeparatingConjunction&) override {}
        void enter(const SeparatingConjunction&) override {}
        void enter(const SeparatingImplication&) override {}
        void enter(const NegatedAxiom&) override {}
        void enter(const BoolAxiom&) override {}
        void enter(const PointsToAxiom&) override {}
        void enter(const StackAxiom&) override {}
        void enter(const StackDisjunction&) override {}
        void enter(const OwnershipAxiom&) override {}
        void enter(const DataStructureLogicallyContainsAxiom&) override {}
        void enter(const NodeLogicallyContainsAxiom&) override {}
        void enter(const KeysetContainsAxiom&) override {}
        void enter(const HasFlowAxiom&) override {}
        void enter(const FlowContainsAxiom&) override {}
        void enter(const UniqueInflowAxiom&) override {}
        void enter(const ObligationAxiom&) override {}
        void enter(const FulfillmentAxiom&) override {}
        void enter(const PastPredicate&) override {}
        void enter(const FuturePredicate&) override {}
        void enter(const Annotation&) override {}

        void exit(const LogicVariable&) override {}
        void exit(const SymbolicBool&) override {}
        void exit(const SymbolicNull&) override {}
        void exit(const SymbolicMin&) override {}
        void exit(const SymbolicMax&) override {}
        void exit(const FlatSeparatingConjunction&) override {}
        void exit(const SeparatingConjunction&) override {}
        void exit(const SeparatingImplication&) override {}
        void exit(const NegatedAxiom&) override {}
        void exit(const BoolAxiom&) override {}
        void exit(const PointsToAxiom&) override {}
        void exit(const StackAxiom&) override {}
        void exit(const StackDisjunction&) override {}
        void exit(const OwnershipAxiom&) override {}
        void exit(const DataStructureLogicallyContainsAxiom&) override {}
        void exit(const NodeLogicallyContainsAxiom&) override {}
        void exit(const KeysetContainsAxiom&) override {}
        void exit(const HasFlowAxiom&) override {}
        void exit(const FlowContainsAxiom&) override {}
        void exit(const UniqueInflowAxiom&) override {}
        void exit(const ObligationAxiom&) override {}
        void exit(const FulfillmentAxiom&) override {}
        void exit(const PastPredicate&) override {}
        void exit(const FuturePredicate&) override {}
        void exit(const Annotation&) override {}
	};

	struct DefaultLogicNonConstListener : public LogicNonConstListener {
        void enter(LogicVariable&) override {}
        void enter(SymbolicBool&) override {}
        void enter(SymbolicNull&) override {}
        void enter(SymbolicMin&) override {}
        void enter(SymbolicMax&) override {}
        void enter(FlatSeparatingConjunction&) override {}
        void enter(SeparatingConjunction&) override {}
        void enter(SeparatingImplication&) override {}
        void enter(NegatedAxiom&) override {}
        void enter(BoolAxiom&) override {}
        void enter(PointsToAxiom&) override {}
        void enter(StackAxiom&) override {}
        void enter(StackDisjunction&) override {}
        void enter(OwnershipAxiom&) override {}
        void enter(DataStructureLogicallyContainsAxiom&) override {}
        void enter(NodeLogicallyContainsAxiom&) override {}
        void enter(KeysetContainsAxiom&) override {}
        void enter(HasFlowAxiom&) override {}
        void enter(FlowContainsAxiom&) override {}
        void enter(UniqueInflowAxiom&) override {}
        void enter(ObligationAxiom&) override {}
        void enter(FulfillmentAxiom&) override {}
        void enter(PastPredicate&) override {}
        void enter(FuturePredicate&) override {}
        void enter(Annotation&) override {}

        void exit(LogicVariable&) override {}
        void exit(SymbolicBool&) override {}
        void exit(SymbolicNull&) override {}
        void exit(SymbolicMin&) override {}
        void exit(SymbolicMax&) override {}
        void exit(FlatSeparatingConjunction&) override {}
        void exit(SeparatingConjunction&) override {}
        void exit(SeparatingImplication&) override {}
        void exit(NegatedAxiom&) override {}
        void exit(BoolAxiom&) override {}
        void exit(PointsToAxiom&) override {}
        void exit(StackAxiom&) override {}
        void exit(StackDisjunction&) override {}
        void exit(OwnershipAxiom&) override {}
        void exit(DataStructureLogicallyContainsAxiom&) override {}
        void exit(NodeLogicallyContainsAxiom&) override {}
        void exit(KeysetContainsAxiom&) override {}
        void exit(HasFlowAxiom&) override {}
        void exit(FlowContainsAxiom&) override {}
        void exit(UniqueInflowAxiom&) override {}
        void exit(ObligationAxiom&) override {}
        void exit(FulfillmentAxiom&) override {}
        void exit(PastPredicate&) override {}
        void exit(FuturePredicate&) override {}
        void exit(Annotation&) override {}
	};

} // namespace heal

#endif