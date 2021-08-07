#include "logics/util.hpp"

using namespace plankton;


template<typename T>
struct ConstCollector : public LogicListener {
    std::set<T*> result;
    const std::function<bool(const T&)>& pickPredicate;
    
    explicit ConstCollector(const std::function<bool(const T&)>& filter) : pickPredicate(filter) {}
    
    template<typename U>
    void Handle(const U& /*object*/) {
        throw std::logic_error("Internal error: 'Collect' failed."); // TODO: better error handling // TODO: this should be removed!?
    }
    
    template<typename U, EnableIfBaseOf<T, U>>
    void Handle(const U& object) {
        if (!pickPredicate(object)) return;
        // result.insert(&object);
        result.insert(const_cast<T*>(&object));
    }
    
    void Visit(const Annotation& /*object*/) override {
        throw std::logic_error("Cautiously refusing to 'Collect' from 'Annotation'."); // TODO: better error handling
    }
    
    void Enter(const VariableDeclaration& object) override { Handle(object); }
    void Enter(const SymbolDeclaration& object) override { Handle(object); }
    void Enter(const SymbolicVariable& object) override { Handle(object); }
    void Enter(const SymbolicBool& object) override { Handle(object); }
    void Enter(const SymbolicNull& object) override { Handle(object); }
    void Enter(const SymbolicMin& object) override { Handle(object); }
    void Enter(const SymbolicMax& object) override { Handle(object); }
    void Enter(const SeparatingConjunction& object) override { Handle(object); }
    void Enter(const LocalMemoryResource& object) override { Handle(object); }
    void Enter(const SharedMemoryCore& object) override { Handle(object); }
    void Enter(const EqualsToAxiom& object) override { Handle(object); }
    void Enter(const StackAxiom& object) override { Handle(object); }
    void Enter(const InflowEmptinessAxiom& object) override { Handle(object); }
    void Enter(const InflowContainsValueAxiom& object) override { Handle(object); }
    void Enter(const InflowContainsRangeAxiom& object) override { Handle(object); }
    void Enter(const ObligationAxiom& object) override { Handle(object); }
    void Enter(const FulfillmentAxiom& object) override { Handle(object); }
    void Enter(const SeparatingImplication& object) override { Handle(object); }
    void Enter(const PastPredicate& object) override { Handle(object); }
    void Enter(const FuturePredicate& object) override { Handle(object); }
    void Enter(const Annotation& object) override { Handle(object); }
};


template<typename T>
std::set<const T*> plankton::Collect(const LogicObject& object, const std::function<bool(const T&)>& filter) {
    ConstCollector<const T> collector(filter);
    object.Accept(collector);
    return std::move(collector.result);
}

template<typename T>
std::set<T*> plankton::CollectMutable(LogicObject& object, const std::function<bool(const T&)>& filter) {
    ConstCollector<T> collector(filter);
    object.Accept(collector);
    return std::move(collector.result);
}


#define CONST_INSTANCE(X) \
    template \
    std::set<const X*> plankton::Collect<X>(const LogicObject& object, const std::function<bool(const X&)>& filter);

CONST_INSTANCE(VariableDeclaration)
CONST_INSTANCE(SymbolDeclaration)
CONST_INSTANCE(SymbolicExpression)
CONST_INSTANCE(SymbolicVariable)
CONST_INSTANCE(SymbolicBool)
CONST_INSTANCE(SymbolicNull)
CONST_INSTANCE(SymbolicMin)
CONST_INSTANCE(SymbolicMax)
CONST_INSTANCE(Formula)
CONST_INSTANCE(Axiom)
CONST_INSTANCE(SeparatingConjunction)
CONST_INSTANCE(MemoryAxiom)
CONST_INSTANCE(LocalMemoryResource)
CONST_INSTANCE(SharedMemoryCore)
CONST_INSTANCE(EqualsToAxiom)
CONST_INSTANCE(StackAxiom)
CONST_INSTANCE(InflowEmptinessAxiom)
CONST_INSTANCE(InflowContainsValueAxiom)
CONST_INSTANCE(InflowContainsRangeAxiom)
CONST_INSTANCE(ObligationAxiom)
CONST_INSTANCE(FulfillmentAxiom)
CONST_INSTANCE(SeparatingImplication)
CONST_INSTANCE(PastPredicate)
CONST_INSTANCE(FuturePredicate)
CONST_INSTANCE(Annotation)


#define MUTABLE_INSTANCE(X) \
    template \
    std::set<X*> plankton::CollectMutable<X>(LogicObject& object, const std::function<bool(const X&)>& filter);

MUTABLE_INSTANCE(VariableDeclaration)
MUTABLE_INSTANCE(SymbolDeclaration)
MUTABLE_INSTANCE(SymbolicExpression)
MUTABLE_INSTANCE(SymbolicVariable)
MUTABLE_INSTANCE(SymbolicBool)
MUTABLE_INSTANCE(SymbolicNull)
MUTABLE_INSTANCE(SymbolicMin)
MUTABLE_INSTANCE(SymbolicMax)
MUTABLE_INSTANCE(Formula)
MUTABLE_INSTANCE(Axiom)
MUTABLE_INSTANCE(SeparatingConjunction)
MUTABLE_INSTANCE(MemoryAxiom)
MUTABLE_INSTANCE(LocalMemoryResource)
MUTABLE_INSTANCE(SharedMemoryCore)
MUTABLE_INSTANCE(EqualsToAxiom)
MUTABLE_INSTANCE(StackAxiom)
MUTABLE_INSTANCE(InflowEmptinessAxiom)
MUTABLE_INSTANCE(InflowContainsValueAxiom)
MUTABLE_INSTANCE(InflowContainsRangeAxiom)
MUTABLE_INSTANCE(ObligationAxiom)
MUTABLE_INSTANCE(FulfillmentAxiom)
MUTABLE_INSTANCE(SeparatingImplication)
MUTABLE_INSTANCE(PastPredicate)
MUTABLE_INSTANCE(FuturePredicate)
MUTABLE_INSTANCE(Annotation)
