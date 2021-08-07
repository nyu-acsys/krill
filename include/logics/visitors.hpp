#pragma once
#ifndef PLANKTON_LOGICS_VISITORS_HPP
#define PLANKTON_LOGICS_VISITORS_HPP

namespace plankton {
    
    // forward declarations
    struct VariableDeclaration;
    struct SymbolDeclaration;
    struct SymbolicVariable;
    struct SymbolicBool;
    struct SymbolicNull;
    struct SymbolicMin;
    struct SymbolicMax;
    struct SeparatingConjunction;
    struct LocalMemoryResource;
    struct SharedMemoryCore;
    struct EqualsToAxiom;
    struct StackAxiom;
    struct InflowEmptinessAxiom;
    struct InflowContainsValueAxiom;
    struct InflowContainsRangeAxiom;
    struct ObligationAxiom;
    struct FulfillmentAxiom;
    struct SeparatingImplication;
    struct PastPredicate;
    struct FuturePredicate;
    struct Annotation;
    
    //
    // Const visitors
    //
    
    struct LogicVisitor {
        virtual void Visit(const SymbolicVariable& object) = 0;
        virtual void Visit(const SymbolicBool& object) = 0;
        virtual void Visit(const SymbolicNull& object) = 0;
        virtual void Visit(const SymbolicMin& object) = 0;
        virtual void Visit(const SymbolicMax& object) = 0;
        virtual void Visit(const SeparatingConjunction& object) = 0;
        virtual void Visit(const LocalMemoryResource& object) = 0;
        virtual void Visit(const SharedMemoryCore& object) = 0;
        virtual void Visit(const EqualsToAxiom& object) = 0;
        virtual void Visit(const StackAxiom& object) = 0;
        virtual void Visit(const InflowEmptinessAxiom& object) = 0;
        virtual void Visit(const InflowContainsValueAxiom& object) = 0;
        virtual void Visit(const InflowContainsRangeAxiom& object) = 0;
        virtual void Visit(const ObligationAxiom& object) = 0;
        virtual void Visit(const FulfillmentAxiom& object) = 0;
        virtual void Visit(const SeparatingImplication& object) = 0;
        virtual void Visit(const PastPredicate& object) = 0;
        virtual void Visit(const FuturePredicate& object) = 0;
        virtual void Visit(const Annotation& object) = 0;
        
        void Walk(const SymbolicVariable& object);
        void Walk(const SymbolicBool& object);
        void Walk(const SymbolicNull& object);
        void Walk(const SymbolicMin& object);
        void Walk(const SymbolicMax& object);
        void Walk(const SeparatingConjunction& object);
        void Walk(const LocalMemoryResource& object);
        void Walk(const SharedMemoryCore& object);
        void Walk(const EqualsToAxiom& object);
        void Walk(const StackAxiom& object);
        void Walk(const InflowEmptinessAxiom& object);
        void Walk(const InflowContainsValueAxiom& object);
        void Walk(const InflowContainsRangeAxiom& object);
        void Walk(const ObligationAxiom& object);
        void Walk(const FulfillmentAxiom& object);
        void Walk(const SeparatingImplication& object);
        void Walk(const PastPredicate& object);
        void Walk(const FuturePredicate& object);
        void Walk(const Annotation& object);
    };
    
    /**
     * LogicVisitor that throws an exception in every function, unless overridden.
     */
    struct BaseLogicVisitor : public LogicVisitor {
        void Visit(const SymbolicVariable& object) override;
        void Visit(const SymbolicBool& object) override;
        void Visit(const SymbolicNull& object) override;
        void Visit(const SymbolicMin& object) override;
        void Visit(const SymbolicMax& object) override;
        void Visit(const SeparatingConjunction& object) override;
        void Visit(const LocalMemoryResource& object) override;
        void Visit(const SharedMemoryCore& object) override;
        void Visit(const EqualsToAxiom& object) override;
        void Visit(const StackAxiom& object) override;
        void Visit(const InflowEmptinessAxiom& object) override;
        void Visit(const InflowContainsValueAxiom& object) override;
        void Visit(const InflowContainsRangeAxiom& object) override;
        void Visit(const ObligationAxiom& object) override;
        void Visit(const FulfillmentAxiom& object) override;
        void Visit(const SeparatingImplication& object) override;
        void Visit(const PastPredicate& object) override;
        void Visit(const FuturePredicate& object) override;
        void Visit(const Annotation& object) override;
    };
    
    /**
     * LogicVisitor that does nothing, unless overridden.
     */
    struct DefaultLogicVisitor : public LogicVisitor {
        void Visit(const SymbolicVariable& object) override;
        void Visit(const SymbolicBool& object) override;
        void Visit(const SymbolicNull& object) override;
        void Visit(const SymbolicMin& object) override;
        void Visit(const SymbolicMax& object) override;
        void Visit(const SeparatingConjunction& object) override;
        void Visit(const LocalMemoryResource& object) override;
        void Visit(const SharedMemoryCore& object) override;
        void Visit(const EqualsToAxiom& object) override;
        void Visit(const StackAxiom& object) override;
        void Visit(const InflowEmptinessAxiom& object) override;
        void Visit(const InflowContainsValueAxiom& object) override;
        void Visit(const InflowContainsRangeAxiom& object) override;
        void Visit(const ObligationAxiom& object) override;
        void Visit(const FulfillmentAxiom& object) override;
        void Visit(const SeparatingImplication& object) override;
        void Visit(const PastPredicate& object) override;
        void Visit(const FuturePredicate& object) override;
        void Visit(const Annotation& object) override;
    };
    
    /**
     * LogicVisitor that walks the AST.
     */
     
    struct LogicListener : public LogicVisitor {
        void Visit(const SymbolicVariable& object) override;
        void Visit(const SymbolicBool& object) override;
        void Visit(const SymbolicNull& object) override;
        void Visit(const SymbolicMin& object) override;
        void Visit(const SymbolicMax& object) override;
        void Visit(const SeparatingConjunction& object) override;
        void Visit(const LocalMemoryResource& object) override;
        void Visit(const SharedMemoryCore& object) override;
        void Visit(const EqualsToAxiom& object) override;
        void Visit(const StackAxiom& object) override;
        void Visit(const InflowEmptinessAxiom& object) override;
        void Visit(const InflowContainsValueAxiom& object) override;
        void Visit(const InflowContainsRangeAxiom& object) override;
        void Visit(const ObligationAxiom& object) override;
        void Visit(const FulfillmentAxiom& object) override;
        void Visit(const SeparatingImplication& object) override;
        void Visit(const PastPredicate& object) override;
        void Visit(const FuturePredicate& object) override;
        void Visit(const Annotation& object) override;
        
        virtual void Enter(const VariableDeclaration& object);
        virtual void Enter(const SymbolDeclaration& object);
        virtual void Enter(const SymbolicVariable& object);
        virtual void Enter(const SymbolicBool& object);
        virtual void Enter(const SymbolicNull& object);
        virtual void Enter(const SymbolicMin& object);
        virtual void Enter(const SymbolicMax& object);
        virtual void Enter(const SeparatingConjunction& object);
        virtual void Enter(const LocalMemoryResource& object);
        virtual void Enter(const SharedMemoryCore& object);
        virtual void Enter(const EqualsToAxiom& object);
        virtual void Enter(const StackAxiom& object);
        virtual void Enter(const InflowEmptinessAxiom& object);
        virtual void Enter(const InflowContainsValueAxiom& object);
        virtual void Enter(const InflowContainsRangeAxiom& object);
        virtual void Enter(const ObligationAxiom& object);
        virtual void Enter(const FulfillmentAxiom& object);
        virtual void Enter(const SeparatingImplication& object);
        virtual void Enter(const PastPredicate& object);
        virtual void Enter(const FuturePredicate& object);
        virtual void Enter(const Annotation& object);
    };
    
    //
    // Non-const visitors
    //
    
    struct MutableLogicVisitor {
        virtual void Visit(SymbolicVariable& object) = 0;
        virtual void Visit(SymbolicBool& object) = 0;
        virtual void Visit(SymbolicNull& object) = 0;
        virtual void Visit(SymbolicMin& object) = 0;
        virtual void Visit(SymbolicMax& object) = 0;
        virtual void Visit(SeparatingConjunction& object) = 0;
        virtual void Visit(LocalMemoryResource& object) = 0;
        virtual void Visit(SharedMemoryCore& object) = 0;
        virtual void Visit(EqualsToAxiom& object) = 0;
        virtual void Visit(StackAxiom& object) = 0;
        virtual void Visit(InflowEmptinessAxiom& object) = 0;
        virtual void Visit(InflowContainsValueAxiom& object) = 0;
        virtual void Visit(InflowContainsRangeAxiom& object) = 0;
        virtual void Visit(ObligationAxiom& object) = 0;
        virtual void Visit(FulfillmentAxiom& object) = 0;
        virtual void Visit(SeparatingImplication& object) = 0;
        virtual void Visit(PastPredicate& object) = 0;
        virtual void Visit(FuturePredicate& object) = 0;
        virtual void Visit(Annotation& object) = 0;
        
        void Walk(SymbolicVariable& object);
        void Walk(SymbolicBool& object);
        void Walk(SymbolicNull& object);
        void Walk(SymbolicMin& object);
        void Walk(SymbolicMax& object);
        void Walk(SeparatingConjunction& object);
        void Walk(LocalMemoryResource& object);
        void Walk(SharedMemoryCore& object);
        void Walk(EqualsToAxiom& object);
        void Walk(StackAxiom& object);
        void Walk(InflowEmptinessAxiom& object);
        void Walk(InflowContainsValueAxiom& object);
        void Walk(InflowContainsRangeAxiom& object);
        void Walk(ObligationAxiom& object);
        void Walk(FulfillmentAxiom& object);
        void Walk(SeparatingImplication& object);
        void Walk(PastPredicate& object);
        void Walk(FuturePredicate& object);
        void Walk(Annotation& object);
    };
    
    /**
     * MutableLogicVisitor that throws an exception in every function, unless overridden.
     */
    struct MutableBaseLogicVisitor : public MutableLogicVisitor {
        void Visit(SymbolicVariable& object) override;
        void Visit(SymbolicBool& object) override;
        void Visit(SymbolicNull& object) override;
        void Visit(SymbolicMin& object) override;
        void Visit(SymbolicMax& object) override;
        void Visit(SeparatingConjunction& object) override;
        void Visit(LocalMemoryResource& object) override;
        void Visit(SharedMemoryCore& object) override;
        void Visit(EqualsToAxiom& object) override;
        void Visit(StackAxiom& object) override;
        void Visit(InflowEmptinessAxiom& object) override;
        void Visit(InflowContainsValueAxiom& object) override;
        void Visit(InflowContainsRangeAxiom& object) override;
        void Visit(ObligationAxiom& object) override;
        void Visit(FulfillmentAxiom& object) override;
        void Visit(SeparatingImplication& object) override;
        void Visit(PastPredicate& object) override;
        void Visit(FuturePredicate& object) override;
        void Visit(Annotation& object) override;
    };
    
    /**
     * MutableLogicVisitor that does nothing, unless overridden.
     */
    struct MutableDefaultLogicVisitor : public MutableLogicVisitor {
        void Visit(SymbolicVariable& object) override;
        void Visit(SymbolicBool& object) override;
        void Visit(SymbolicNull& object) override;
        void Visit(SymbolicMin& object) override;
        void Visit(SymbolicMax& object) override;
        void Visit(SeparatingConjunction& object) override;
        void Visit(LocalMemoryResource& object) override;
        void Visit(SharedMemoryCore& object) override;
        void Visit(EqualsToAxiom& object) override;
        void Visit(StackAxiom& object) override;
        void Visit(InflowEmptinessAxiom& object) override;
        void Visit(InflowContainsValueAxiom& object) override;
        void Visit(InflowContainsRangeAxiom& object) override;
        void Visit(ObligationAxiom& object) override;
        void Visit(FulfillmentAxiom& object) override;
        void Visit(SeparatingImplication& object) override;
        void Visit(PastPredicate& object) override;
        void Visit(FuturePredicate& object) override;
        void Visit(Annotation& object) override;
    };
    
    /**
     * MutableLogicVisitor that walks the AST.
     */
    struct MutableLogicListener : public MutableLogicVisitor {
        void Visit(SymbolicVariable& object) override;
        void Visit(SymbolicBool& object) override;
        void Visit(SymbolicNull& object) override;
        void Visit(SymbolicMin& object) override;
        void Visit(SymbolicMax& object) override;
        void Visit(SeparatingConjunction& object) override;
        void Visit(LocalMemoryResource& object) override;
        void Visit(SharedMemoryCore& object) override;
        void Visit(EqualsToAxiom& object) override;
        void Visit(StackAxiom& object) override;
        void Visit(InflowEmptinessAxiom& object) override;
        void Visit(InflowContainsValueAxiom& object) override;
        void Visit(InflowContainsRangeAxiom& object) override;
        void Visit(ObligationAxiom& object) override;
        void Visit(FulfillmentAxiom& object) override;
        void Visit(SeparatingImplication& object) override;
        void Visit(PastPredicate& object) override;
        void Visit(FuturePredicate& object) override;
        void Visit(Annotation& object) override;
        
        virtual void Enter(const VariableDeclaration& object);
        virtual void Enter(const SymbolDeclaration& object);
        virtual void Enter(SymbolicVariable& object);
        virtual void Enter(SymbolicBool& object);
        virtual void Enter(SymbolicNull& object);
        virtual void Enter(SymbolicMin& object);
        virtual void Enter(SymbolicMax& object);
        virtual void Enter(SeparatingConjunction& object);
        virtual void Enter(LocalMemoryResource& object);
        virtual void Enter(SharedMemoryCore& object);
        virtual void Enter(EqualsToAxiom& object);
        virtual void Enter(StackAxiom& object);
        virtual void Enter(InflowEmptinessAxiom& object);
        virtual void Enter(InflowContainsValueAxiom& object);
        virtual void Enter(InflowContainsRangeAxiom& object);
        virtual void Enter(ObligationAxiom& object);
        virtual void Enter(FulfillmentAxiom& object);
        virtual void Enter(SeparatingImplication& object);
        virtual void Enter(PastPredicate& object);
        virtual void Enter(FuturePredicate& object);
        virtual void Enter(Annotation& object);
    };
    
} // namespace plankton

#endif //PLANKTON_LOGICS_VISITORS_HPP
