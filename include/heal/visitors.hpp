#pragma once
#ifndef HEAL_VISITORS
#define HEAL_VISITORS


#include "cola/visitors.hpp"


namespace heal {

    struct LogicObject;
    struct SymbolicVariableDeclaration;
    struct SymbolicFlowDeclaration;
    struct SymbolicExpression;
    struct Formula;
    struct Axiom;
    struct SpecificationAxiom;
    struct TimePredicate;

    struct SymbolicVariable;
    struct SymbolicBool;
    struct SymbolicNull;
    struct SymbolicMin;
    struct SymbolicMax;
    struct SeparatingConjunction;
    struct SeparatingImplication;
    struct PointsToAxiom;
    struct EqualsToAxiom;
    struct SymbolicAxiom;
    struct StackDisjunction;
    struct InflowContainsValueAxiom;
    struct InflowContainsRangeAxiom;
    struct InflowEmptinessAxiom;
    struct ObligationAxiom;
    struct FulfillmentAxiom;
    struct PastPredicate;
    struct FuturePredicate;
    struct Annotation;


	struct LogicVisitor {
        virtual void visit(const SymbolicVariable& object) = 0;
        virtual void visit(const SymbolicBool& object) = 0;
        virtual void visit(const SymbolicNull& object) = 0;
        virtual void visit(const SymbolicMin& object) = 0;
        virtual void visit(const SymbolicMax& object) = 0;
        virtual void visit(const SeparatingConjunction& object) = 0;
        virtual void visit(const SeparatingImplication& object) = 0;
        virtual void visit(const PointsToAxiom& object) = 0;
        virtual void visit(const EqualsToAxiom& object) = 0;
        virtual void visit(const SymbolicAxiom& object) = 0;
        virtual void visit(const StackDisjunction& object) = 0;
        virtual void visit(const InflowContainsValueAxiom& object) = 0;
        virtual void visit(const InflowContainsRangeAxiom& object) = 0;
        virtual void visit(const InflowEmptinessAxiom& object) = 0;
        virtual void visit(const ObligationAxiom& object) = 0;
        virtual void visit(const FulfillmentAxiom& object) = 0;
        virtual void visit(const PastPredicate& object) = 0;
        virtual void visit(const FuturePredicate& object) = 0;
        virtual void visit(const Annotation& object) = 0;
    };

	struct LogicNonConstVisitor {
        virtual void visit(SymbolicVariable& object) = 0;
        virtual void visit(SymbolicBool& object) = 0;
        virtual void visit(SymbolicNull& object) = 0;
        virtual void visit(SymbolicMin& object) = 0;
        virtual void visit(SymbolicMax& object) = 0;
        virtual void visit(SeparatingConjunction& object) = 0;
        virtual void visit(SeparatingImplication& object) = 0;
        virtual void visit(PointsToAxiom& object) = 0;
        virtual void visit(EqualsToAxiom& object) = 0;
        virtual void visit(SymbolicAxiom& object) = 0;
        virtual void visit(StackDisjunction& object) = 0;
        virtual void visit(InflowContainsValueAxiom& object) = 0;
        virtual void visit(InflowContainsRangeAxiom& object) = 0;
        virtual void visit(InflowEmptinessAxiom& object) = 0;
        virtual void visit(ObligationAxiom& object) = 0;
        virtual void visit(FulfillmentAxiom& object) = 0;
        virtual void visit(PastPredicate& object) = 0;
        virtual void visit(FuturePredicate& object) = 0;
        virtual void visit(Annotation& object) = 0;
	};


	struct BaseLogicVisitor : public LogicVisitor {
        void visit(const SymbolicVariable&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicVariable&"); }
        void visit(const SymbolicBool&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicBool&"); }
        void visit(const SymbolicNull&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicNull&"); }
        void visit(const SymbolicMin&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicMin&"); }
        void visit(const SymbolicMax&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicMax&"); }
        void visit(const SeparatingConjunction&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SeparatingConjunction&"); }
        void visit(const SeparatingImplication&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SeparatingImplication&"); }
        void visit(const PointsToAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const PointsToAxiom&"); }
        void visit(const EqualsToAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const EqualsToAxiom&"); }
        void visit(const SymbolicAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const SymbolicAxiom&"); }
        void visit(const StackDisjunction&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const StackDisjunction&"); }
        void visit(const InflowContainsValueAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const InflowContainsValueAxiom&"); }
        void visit(const InflowContainsRangeAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const InflowContainsRangeAxiom&"); }
        void visit(const InflowEmptinessAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const InflowEmptinessAxiom&"); }
        void visit(const ObligationAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const ObligationAxiom&"); }
        void visit(const FulfillmentAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FulfillmentAxiom&"); }
        void visit(const PastPredicate&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const PastPredicate&"); }
        void visit(const FuturePredicate&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const FuturePredicate&"); }
        void visit(const Annotation&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicVisitor", "const Annotation&"); }
	};

	struct BaseLogicNonConstVisitor : public LogicNonConstVisitor {
        void visit(SymbolicVariable&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SymbolicVariable&"); }
        void visit(SymbolicBool&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SymbolicBool&"); }
        void visit(SymbolicNull&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SymbolicNull&"); }
        void visit(SymbolicMin&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SymbolicMin&"); }
        void visit(SymbolicMax&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SymbolicMax&"); }
        void visit(SeparatingConjunction&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SeparatingConjunction&"); }
        void visit(SeparatingImplication&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SeparatingImplication&"); }
        void visit(PointsToAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "PointsToAxiom&"); }
        void visit(EqualsToAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "EqualsToAxiom&"); }
        void visit(SymbolicAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "SymbolicAxiom&"); }
        void visit(StackDisjunction&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "StackDisjunction&"); }
        void visit(InflowContainsValueAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "InflowContainsValueAxiom&"); }
        void visit(InflowContainsRangeAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "InflowContainsRangeAxiom&"); }
        void visit(InflowEmptinessAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "InflowEmptinessAxiom&"); }
        void visit(ObligationAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "ObligationAxiom&"); }
        void visit(FulfillmentAxiom&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "FulfillmentAxiom&"); }
        void visit(PastPredicate&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "PastPredicate&"); }
        void visit(FuturePredicate&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "FuturePredicate&"); }
        void visit(Annotation&) override { throw cola::VisitorNotImplementedError(*this, "BaseLogicNonConstVisitor", "Annotation&"); }
	};


	struct LogicListener : public LogicVisitor {
	    // visit
        void visit(const SymbolicVariable& object) override;
        void visit(const SymbolicBool& object) override;
        void visit(const SymbolicNull& object) override;
        void visit(const SymbolicMin& object) override;
        void visit(const SymbolicMax& object) override;
        void visit(const SeparatingConjunction& object) override;
        void visit(const SeparatingImplication& object) override;
        void visit(const PointsToAxiom& object) override;
        void visit(const EqualsToAxiom& object) override;
        void visit(const SymbolicAxiom& object) override;
        void visit(const StackDisjunction& object) override;
        void visit(const InflowContainsValueAxiom& object) override;
        void visit(const InflowContainsRangeAxiom& object) override;
        void visit(const InflowEmptinessAxiom& object) override;
        void visit(const ObligationAxiom& object) override;
        void visit(const FulfillmentAxiom& object) override;
        void visit(const PastPredicate& object) override;
        void visit(const FuturePredicate& object) override;
        void visit(const Annotation& object) override;

        // callback
        virtual void enter(const SymbolicVariable& object) = 0;
        virtual void enter(const SymbolicBool& object) = 0;
        virtual void enter(const SymbolicNull& object) = 0;
        virtual void enter(const SymbolicMin& object) = 0;
        virtual void enter(const SymbolicMax& object) = 0;
        virtual void enter(const SeparatingConjunction& object) = 0;
        virtual void enter(const SeparatingImplication& object) = 0;
        virtual void enter(const PointsToAxiom& object) = 0;
        virtual void enter(const EqualsToAxiom& object) = 0;
        virtual void enter(const SymbolicAxiom& object) = 0;
        virtual void enter(const StackDisjunction& object) = 0;
        virtual void enter(const InflowContainsValueAxiom& object) = 0;
        virtual void enter(const InflowContainsRangeAxiom& object) = 0;
        virtual void enter(const InflowEmptinessAxiom& object) = 0;
        virtual void enter(const ObligationAxiom& object) = 0;
        virtual void enter(const FulfillmentAxiom& object) = 0;
        virtual void enter(const PastPredicate& object) = 0;
        virtual void enter(const FuturePredicate& object) = 0;
        virtual void enter(const Annotation& object) = 0;

        virtual void exit(const SymbolicVariable& object) = 0;
        virtual void exit(const SymbolicBool& object) = 0;
        virtual void exit(const SymbolicNull& object) = 0;
        virtual void exit(const SymbolicMin& object) = 0;
        virtual void exit(const SymbolicMax& object) = 0;
        virtual void exit(const SeparatingConjunction& object) = 0;
        virtual void exit(const SeparatingImplication& object) = 0;
        virtual void exit(const PointsToAxiom& object) = 0;
        virtual void exit(const EqualsToAxiom& object) = 0;
        virtual void exit(const SymbolicAxiom& object) = 0;
        virtual void exit(const StackDisjunction& object) = 0;
        virtual void exit(const InflowContainsValueAxiom& object) = 0;
        virtual void exit(const InflowContainsRangeAxiom& object) = 0;
        virtual void exit(const InflowEmptinessAxiom& object) = 0;
        virtual void exit(const ObligationAxiom& object) = 0;
        virtual void exit(const FulfillmentAxiom& object) = 0;
        virtual void exit(const PastPredicate& object) = 0;
        virtual void exit(const FuturePredicate& object) = 0;
        virtual void exit(const Annotation& object) = 0;
	};

	struct LogicNonConstListener : public LogicNonConstVisitor {
	    // visit
        void visit(SymbolicVariable& object) override;
        void visit(SymbolicBool& object) override;
        void visit(SymbolicNull& object) override;
        void visit(SymbolicMin& object) override;
        void visit(SymbolicMax& object) override;
        void visit(SeparatingConjunction& object) override;
        void visit(SeparatingImplication& object) override;
        void visit(PointsToAxiom& object) override;
        void visit(EqualsToAxiom& object) override;
        void visit(SymbolicAxiom& object) override;
        void visit(StackDisjunction& object) override;
        void visit(InflowContainsValueAxiom& object) override;
        void visit(InflowContainsRangeAxiom& object) override;
        void visit(InflowEmptinessAxiom& object) override;
        void visit(ObligationAxiom& object) override;
        void visit(FulfillmentAxiom& object) override;
        void visit(PastPredicate& object) override;
        void visit(FuturePredicate& object) override;
        void visit(Annotation& object) override;

        // callback
        virtual void enter(SymbolicVariable& object) = 0;
        virtual void enter(SymbolicBool& object) = 0;
        virtual void enter(SymbolicNull& object) = 0;
        virtual void enter(SymbolicMin& object) = 0;
        virtual void enter(SymbolicMax& object) = 0;
        virtual void enter(SeparatingConjunction& object) = 0;
        virtual void enter(SeparatingImplication& object) = 0;
        virtual void enter(PointsToAxiom& object) = 0;
        virtual void enter(EqualsToAxiom& object) = 0;
        virtual void enter(SymbolicAxiom& object) = 0;
        virtual void enter(StackDisjunction& object) = 0;
        virtual void enter(InflowContainsValueAxiom& object) = 0;
        virtual void enter(InflowContainsRangeAxiom& object) = 0;
        virtual void enter(InflowEmptinessAxiom& object) = 0;
        virtual void enter(ObligationAxiom& object) = 0;
        virtual void enter(FulfillmentAxiom& object) = 0;
        virtual void enter(PastPredicate& object) = 0;
        virtual void enter(FuturePredicate& object) = 0;
        virtual void enter(Annotation& object) = 0;

        virtual void exit(SymbolicVariable& object) = 0;
        virtual void exit(SymbolicBool& object) = 0;
        virtual void exit(SymbolicNull& object) = 0;
        virtual void exit(SymbolicMin& object) = 0;
        virtual void exit(SymbolicMax& object) = 0;
        virtual void exit(SeparatingConjunction& object) = 0;
        virtual void exit(SeparatingImplication& object) = 0;
        virtual void exit(PointsToAxiom& object) = 0;
        virtual void exit(EqualsToAxiom& object) = 0;
        virtual void exit(SymbolicAxiom& object) = 0;
        virtual void exit(StackDisjunction& object) = 0;
        virtual void exit(InflowContainsValueAxiom& object) = 0;
        virtual void exit(InflowContainsRangeAxiom& object) = 0;
        virtual void exit(InflowEmptinessAxiom& object) = 0;
        virtual void exit(ObligationAxiom& object) = 0;
        virtual void exit(FulfillmentAxiom& object) = 0;
        virtual void exit(PastPredicate& object) = 0;
        virtual void exit(FuturePredicate& object) = 0;
        virtual void exit(Annotation& object) = 0;
	};


	struct DefaultLogicListener : public LogicListener {
        void enter(const SymbolicVariable&) override {}
        void enter(const SymbolicBool&) override {}
        void enter(const SymbolicNull&) override {}
        void enter(const SymbolicMin&) override {}
        void enter(const SymbolicMax&) override {}
        void enter(const SeparatingConjunction&) override {}
        void enter(const SeparatingImplication&) override {}
        void enter(const PointsToAxiom&) override {}
        void enter(const EqualsToAxiom&) override {}
        void enter(const SymbolicAxiom&) override {}
        void enter(const StackDisjunction&) override {}
        void enter(const InflowContainsValueAxiom&) override {}
        void enter(const InflowContainsRangeAxiom&) override {}
        void enter(const InflowEmptinessAxiom&) override {}
        void enter(const ObligationAxiom&) override {}
        void enter(const FulfillmentAxiom&) override {}
        void enter(const PastPredicate&) override {}
        void enter(const FuturePredicate&) override {}
        void enter(const Annotation&) override {}

        void exit(const SymbolicVariable&) override {}
        void exit(const SymbolicBool&) override {}
        void exit(const SymbolicNull&) override {}
        void exit(const SymbolicMin&) override {}
        void exit(const SymbolicMax&) override {}
        void exit(const SeparatingConjunction&) override {}
        void exit(const SeparatingImplication&) override {}
        void exit(const PointsToAxiom&) override {}
        void exit(const EqualsToAxiom&) override {}
        void exit(const SymbolicAxiom&) override {}
        void exit(const StackDisjunction&) override {}
        void exit(const InflowContainsValueAxiom&) override {}
        void exit(const InflowContainsRangeAxiom&) override {}
        void exit(const InflowEmptinessAxiom&) override {}
        void exit(const ObligationAxiom&) override {}
        void exit(const FulfillmentAxiom&) override {}
        void exit(const PastPredicate&) override {}
        void exit(const FuturePredicate&) override {}
        void exit(const Annotation&) override {}
	};

	struct DefaultLogicNonConstListener : public LogicNonConstListener {
        void enter(SymbolicVariable&) override {}
        void enter(SymbolicBool&) override {}
        void enter(SymbolicNull&) override {}
        void enter(SymbolicMin&) override {}
        void enter(SymbolicMax&) override {}
        void enter(SeparatingConjunction&) override {}
        void enter(SeparatingImplication&) override {}
        void enter(PointsToAxiom&) override {}
        void enter(EqualsToAxiom&) override {}
        void enter(SymbolicAxiom&) override {}
        void enter(StackDisjunction&) override {}
        void enter(InflowContainsValueAxiom&) override {}
        void enter(InflowContainsRangeAxiom&) override {}
        void enter(InflowEmptinessAxiom&) override {}
        void enter(ObligationAxiom&) override {}
        void enter(FulfillmentAxiom&) override {}
        void enter(PastPredicate&) override {}
        void enter(FuturePredicate&) override {}
        void enter(Annotation&) override {}

        void exit(SymbolicVariable&) override {}
        void exit(SymbolicBool&) override {}
        void exit(SymbolicNull&) override {}
        void exit(SymbolicMin&) override {}
        void exit(SymbolicMax&) override {}
        void exit(SeparatingConjunction&) override {}
        void exit(SeparatingImplication&) override {}
        void exit(PointsToAxiom&) override {}
        void exit(EqualsToAxiom&) override {}
        void exit(SymbolicAxiom&) override {}
        void exit(StackDisjunction&) override {}
        void exit(InflowContainsValueAxiom&) override {}
        void exit(InflowContainsRangeAxiom&) override {}
        void exit(InflowEmptinessAxiom&) override {}
        void exit(ObligationAxiom&) override {}
        void exit(FulfillmentAxiom&) override {}
        void exit(PastPredicate&) override {}
        void exit(FuturePredicate&) override {}
        void exit(Annotation&) override {}
	};

} // namespace heal

#endif