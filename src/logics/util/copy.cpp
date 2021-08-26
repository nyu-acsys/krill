#include "logics/util.hpp"

#include "programs/util.hpp"

using namespace plankton;


//
// Handling superclasses
//

template<typename T>
struct CopyVisitor : public LogicVisitor {
    std::unique_ptr<T> result;
    
    static std::unique_ptr<T> Copy(const T& object) {
        CopyVisitor<T> visitor;
        object.Accept(visitor);
        assert(visitor.result);
        return std::move(visitor.result);
    }
    
    template<typename U>
    void Handle(const U& /*object*/) {
        throw std::logic_error("Internal error: 'Copy' failed."); // TODO: better error handling
    }
    
    template<typename U, EnableIfBaseOf<T, U>>
    void Handle(const U& object) {
        result = plankton::Copy(object);
    }
    
    void Visit(const SymbolicVariable& object) override { Handle(object); }
    void Visit(const SymbolicBool& object) override { Handle(object); }
    void Visit(const SymbolicNull& object) override { Handle(object); }
    void Visit(const SymbolicMin& object) override { Handle(object); }
    void Visit(const SymbolicMax& object) override { Handle(object); }
    void Visit(const SeparatingConjunction& object) override { Handle(object); }
    void Visit(const LocalMemoryResource& object) override { Handle(object); }
    void Visit(const SharedMemoryCore& object) override { Handle(object); }
    void Visit(const EqualsToAxiom& object) override { Handle(object); }
    void Visit(const StackAxiom& object) override { Handle(object); }
    void Visit(const InflowEmptinessAxiom& object) override { Handle(object); }
    void Visit(const InflowContainsValueAxiom& object) override { Handle(object); }
    void Visit(const InflowContainsRangeAxiom& object) override { Handle(object); }
    void Visit(const ObligationAxiom& object) override { Handle(object); }
    void Visit(const FulfillmentAxiom& object) override { Handle(object); }
    void Visit(const SeparatingImplication& object) override { Handle(object); }
    void Visit(const Invariant& object) override { Handle(object); }
    void Visit(const PastPredicate& object) override { Handle(object); }
    void Visit(const FuturePredicate& object) override { Handle(object); }
    void Visit(const Annotation& object) override { Handle(object); }
};

template<>
std::unique_ptr<LogicObject> plankton::Copy<LogicObject>(const LogicObject& object) {
    return CopyVisitor<LogicObject>::Copy(object);
}

template<>
std::unique_ptr<Formula> plankton::Copy<Formula>(const Formula& object) {
    return CopyVisitor<Formula>::Copy(object);
}

template<>
std::unique_ptr<SymbolicExpression> plankton::Copy<SymbolicExpression>(const SymbolicExpression& object) {
    return CopyVisitor<SymbolicExpression>::Copy(object);
}

template<>
std::unique_ptr<MemoryAxiom> plankton::Copy<MemoryAxiom>(const MemoryAxiom& object) {
    return CopyVisitor<MemoryAxiom>::Copy(object);
}


//
// Copying objects
//

template<>
std::unique_ptr<SymbolicVariable> plankton::Copy<SymbolicVariable>(const SymbolicVariable& object) {
    return std::make_unique<SymbolicVariable>(object.Decl());
}

template<>
std::unique_ptr<SymbolicBool> plankton::Copy<SymbolicBool>(const SymbolicBool& object) {
    return std::make_unique<SymbolicBool>(object.value);
}

template<>
std::unique_ptr<SymbolicNull> plankton::Copy<SymbolicNull>(const SymbolicNull& /*object*/) {
    return std::make_unique<SymbolicNull>();
}

template<>
std::unique_ptr<SymbolicMin> plankton::Copy<SymbolicMin>(const SymbolicMin& /*object*/) {
    return std::make_unique<SymbolicMin>();
}

template<>
std::unique_ptr<SymbolicMax> plankton::Copy<SymbolicMax>(const SymbolicMax& /*object*/) {
    return std::make_unique<SymbolicMax>();
}

template<>
std::unique_ptr<SeparatingConjunction> plankton::Copy<SeparatingConjunction>(const SeparatingConjunction& object) {
    auto result = std::make_unique<SeparatingConjunction>();
    for (const auto& elem : object.conjuncts) result->conjuncts.push_back(plankton::Copy(*elem));
    return result;
}

template<typename T, typename = EnableIfBaseOf<MemoryAxiom, T>>
std::unique_ptr<T> CopyMemoryAxiom(const T& object) {
    auto adr = plankton::Copy(*object.node);
    auto flow = plankton::Copy(*object.flow);
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& [field, value] : object.fieldToValue) fields[field] = plankton::Copy(*value);
    return std::make_unique<T>(std::move(adr), std::move(flow), std::move(fields));
}

template<>
std::unique_ptr<LocalMemoryResource> plankton::Copy<LocalMemoryResource>(const LocalMemoryResource& object) {
    return CopyMemoryAxiom(object);
}

template<>
std::unique_ptr<SharedMemoryCore> plankton::Copy<SharedMemoryCore>(const SharedMemoryCore& object) {
    return CopyMemoryAxiom(object);
}

template<>
std::unique_ptr<EqualsToAxiom> plankton::Copy<EqualsToAxiom>(const EqualsToAxiom& object) {
    return std::make_unique<EqualsToAxiom>(object.Variable(), plankton::Copy(*object.value));
}

template<>
std::unique_ptr<StackAxiom> plankton::Copy<StackAxiom>(const StackAxiom& object) {
    return std::make_unique<StackAxiom>(object.op, plankton::Copy(*object.lhs), plankton::Copy(*object.rhs));
}

template<>
std::unique_ptr<InflowEmptinessAxiom> plankton::Copy<InflowEmptinessAxiom>(const InflowEmptinessAxiom& object) {
    return std::make_unique<InflowEmptinessAxiom>(plankton::Copy(*object.flow), object.isEmpty);
}

template<>
std::unique_ptr<InflowContainsValueAxiom> plankton::Copy<InflowContainsValueAxiom>(const InflowContainsValueAxiom& object) {
    return std::make_unique<InflowContainsValueAxiom>(plankton::Copy(*object.flow), plankton::Copy(*object.value));
}

template<>
std::unique_ptr<InflowContainsRangeAxiom> plankton::Copy<InflowContainsRangeAxiom>(const InflowContainsRangeAxiom& object) {
    return std::make_unique<InflowContainsRangeAxiom>(plankton::Copy(*object.flow),
                                                      plankton::Copy(*object.valueLow),
                                                      plankton::Copy(*object.valueHigh));
}

template<>
std::unique_ptr<ObligationAxiom> plankton::Copy<ObligationAxiom>(const ObligationAxiom& object) {
    return std::make_unique<ObligationAxiom>(object.spec, plankton::Copy(*object.key));
}

template<>
std::unique_ptr<FulfillmentAxiom> plankton::Copy<FulfillmentAxiom>(const FulfillmentAxiom& object) {
    return std::make_unique<FulfillmentAxiom>(object.returnValue);
}

template<>
std::unique_ptr<SeparatingImplication> plankton::Copy<SeparatingImplication>(const SeparatingImplication& object) {
    return std::make_unique<SeparatingImplication>(plankton::Copy(*object.premise), plankton::Copy(*object.conclusion));
}

template<>
std::unique_ptr<PastPredicate> plankton::Copy<PastPredicate>(const PastPredicate& object) {
    return std::make_unique<PastPredicate>(plankton::Copy(*object.formula));
}

template<>
std::unique_ptr<FuturePredicate> plankton::Copy<FuturePredicate>(const FuturePredicate& object) {
    return std::make_unique<FuturePredicate>(object.command, plankton::Copy(*object.pre), plankton::Copy(*object.post));
}

template<>
std::unique_ptr<Annotation> plankton::Copy<Annotation>(const Annotation& object) {
    auto result = std::make_unique<Annotation>(plankton::Copy(*object.now));
    for (const auto& elem : object.past) result->past.push_back(plankton::Copy(*elem));
    for (const auto& elem : object.future) result->future.push_back(plankton::Copy(*elem));
    return result;
}
