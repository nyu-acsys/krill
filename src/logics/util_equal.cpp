#include "logics/util.hpp"

using namespace plankton;


static inline BinaryOperator Symmetric(BinaryOperator op) {
    switch (op) {
        case BinaryOperator::EQ: return BinaryOperator::EQ;
        case BinaryOperator::NEQ: return BinaryOperator::NEQ;
        case BinaryOperator::LEQ: return BinaryOperator::GEQ;
        case BinaryOperator::LT: return BinaryOperator::GT;
        case BinaryOperator::GEQ: return BinaryOperator::LEQ;
        case BinaryOperator::GT: return BinaryOperator::LT;
    }
}

static inline bool IsEqual(const SymbolicNull& /*object*/, const SymbolicNull& /*other*/) { return true; }
static inline bool IsEqual(const SymbolicMin& /*object*/, const SymbolicMin& /*other*/) { return true; }
static inline bool IsEqual(const SymbolicMax& /*object*/, const SymbolicMax& /*other*/) { return true; }
static inline bool IsEqual(const SymbolicVariable& object, const SymbolicVariable& other) {
    return &object.Decl() == &other.Decl();
}
static inline bool IsEqual(const SymbolicBool& object, const SymbolicBool& other) {
    return object.value == other.value;
}

static inline bool IsEqual(const MemoryAxiom& object, const MemoryAxiom& other) {
    auto fieldIsEqual = [&other](const auto& pair) {
        return IsEqual(*pair.second, *other.fieldToValue.at(pair.first));
    };
    return IsEqual(*object.node, *other.node) && IsEqual(*object.flow, *other.flow)
           && std::all_of(object.fieldToValue.begin(), object.fieldToValue.end(), fieldIsEqual);
}
static inline bool IsEqual(const EqualsToAxiom& object, const EqualsToAxiom& other) {
    return &object.variable->Decl() == &other.variable->Decl() && IsEqual(*object.value, *other.value);
}
static inline bool IsEqual(const StackAxiom& object, const StackAxiom& other) {
    return (
                   object.op == other.op
                   && plankton::SyntacticalEqual(*object.lhs, *other.lhs)
                   && plankton::SyntacticalEqual(*object.rhs, *other.rhs)
           ) || (
                   object.op == Symmetric(other.op)
                   && other.op != Symmetric(other.op)
                   && plankton::SyntacticalEqual(*object.lhs, *other.rhs)
                   && plankton::SyntacticalEqual(*object.rhs, *other.lhs)
           );
}
static inline bool IsEqual(const InflowEmptinessAxiom& object, const InflowEmptinessAxiom& other) {
    return object.isEmpty == other.isEmpty && IsEqual(*object.flow, *other.flow);
}
static inline bool IsEqual(const InflowContainsValueAxiom& object, const InflowContainsValueAxiom& other) {
    return IsEqual(*object.flow, *other.flow) && IsEqual(*object.value, *other.value);
}
static inline bool IsEqual(const InflowContainsRangeAxiom& object, const InflowContainsRangeAxiom& other) {
    return IsEqual(*object.flow, *other.flow)
           && plankton::SyntacticalEqual(*object.valueLow, *other.valueLow)
           && plankton::SyntacticalEqual(*object.valueHigh, *other.valueHigh);
}
static inline bool IsEqual(const ObligationAxiom& object, const ObligationAxiom& other) {
    return object.kind == other.kind && IsEqual(*object.key, *other.key);
}
static inline bool IsEqual(const FulfillmentAxiom& object, const FulfillmentAxiom& other) {
    return object.returnValue == other.returnValue;
}

static inline bool IsEqual(const SeparatingConjunction& object, const SeparatingConjunction& other) {
    if (object.conjuncts.size() != other.conjuncts.size()) return false;
    for (std::size_t index = 0; index < object.conjuncts.size(); ++index) {
        if (!plankton::SyntacticalEqual(*object.conjuncts.at(index), *other.conjuncts.at(index))) return false;
    }
    return true;
}

struct Comparator : public BaseLogicVisitor {
    bool result = false;
    const LogicObject& compare;
    
    explicit Comparator(const LogicObject& compare) : compare(compare) {}
    
    template<typename T>
    inline void Compare(const T& object) {
        if (auto other = dynamic_cast<const T*>(&compare)) {
            result = IsEqual(object, *other);
        }
    }
    
    void Visit(const SymbolicVariable& object) override { Compare(object); }
    void Visit(const SymbolicBool& object) override { Compare(object); }
    void Visit(const SymbolicNull& object) override { Compare(object); }
    void Visit(const SymbolicMin& object) override { Compare(object); }
    void Visit(const SymbolicMax& object) override { Compare(object); }
    void Visit(const SeparatingConjunction& object) override { Compare(object); }
    void Visit(const LocalMemoryResource& object) override { Compare(object); }
    void Visit(const SharedMemoryCore& object) override { Compare(object); }
    void Visit(const EqualsToAxiom& object) override { Compare(object); }
    void Visit(const StackAxiom& object) override { Compare(object); }
    void Visit(const InflowEmptinessAxiom& object) override { Compare(object); }
    void Visit(const InflowContainsValueAxiom& object) override { Compare(object); }
    void Visit(const InflowContainsRangeAxiom& object) override { Compare(object); }
    void Visit(const ObligationAxiom& object) override { Compare(object); }
    void Visit(const FulfillmentAxiom& object) override { Compare(object); }
};

inline bool Compare(const LogicObject& object, const LogicObject& other) {
    Comparator comparator(other);
    object.Accept(comparator);
    return comparator.result;
}

bool plankton::SyntacticalEqual(const SymbolicExpression& object, const SymbolicExpression& other) {
    return Compare(object, other);
}

bool plankton::SyntacticalEqual(const Formula& object, const Formula& other) {
    return Compare(object, other);
}
