#include "logics/util.hpp"

using namespace plankton;


inline bool IsEqual(const SymbolicNull& /*object*/, const SymbolicNull& /*other*/) { return true; }
inline bool IsEqual(const SymbolicMin& /*object*/, const SymbolicMin& /*other*/) { return true; }
inline bool IsEqual(const SymbolicMax& /*object*/, const SymbolicMax& /*other*/) { return true; }
inline bool IsEqual(const SymbolicVariable& object, const SymbolicVariable& other) {
    return &object.Decl() == &other.Decl();
}
inline bool IsEqual(const SymbolicBool& object, const SymbolicBool& other) {
    return object.value == other.value;
}

inline bool IsEqual(const MemoryAxiom& object, const MemoryAxiom& other) {
    auto fieldIsEqual = [&other](const auto& pair) {
        return IsEqual(*pair.second, *other.fieldToValue.at(pair.first));
    };
    return IsEqual(*object.node, *other.node) && IsEqual(*object.flow, *other.flow)
           && std::all_of(object.fieldToValue.begin(), object.fieldToValue.end(), fieldIsEqual);
}
inline bool IsEqual(const EqualsToAxiom& object, const EqualsToAxiom& other) {
    return &object.Variable() == &other.Variable() && IsEqual(*object.value, *other.value);
}
inline bool IsEqual(const StackAxiom& object, const StackAxiom& other) {
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
inline bool IsEqual(const InflowEmptinessAxiom& object, const InflowEmptinessAxiom& other) {
    return object.isEmpty == other.isEmpty && IsEqual(*object.flow, *other.flow);
}
inline bool IsEqual(const InflowContainsValueAxiom& object, const InflowContainsValueAxiom& other) {
    return IsEqual(*object.flow, *other.flow) && IsEqual(*object.value, *other.value);
}
inline bool IsEqual(const InflowContainsRangeAxiom& object, const InflowContainsRangeAxiom& other) {
    return IsEqual(*object.flow, *other.flow)
           && plankton::SyntacticalEqual(*object.valueLow, *other.valueLow)
           && plankton::SyntacticalEqual(*object.valueHigh, *other.valueHigh);
}
inline bool IsEqual(const ObligationAxiom& object, const ObligationAxiom& other) {
    return object.spec == other.spec && IsEqual(*object.key, *other.key);
}
inline bool IsEqual(const FulfillmentAxiom& object, const FulfillmentAxiom& other) {
    return object.returnValue == other.returnValue;
}

inline bool IsEqual(const SeparatingConjunction& object, const SeparatingConjunction& other) {
    if (object.conjuncts.size() != other.conjuncts.size()) return false;
    for (std::size_t index = 0; index < object.conjuncts.size(); ++index) {
        if (!plankton::SyntacticalEqual(*object.conjuncts.at(index), *other.conjuncts.at(index))) return false;
    }
    return true;
}

inline bool IsEqual(const SeparatingImplication& object, const SeparatingImplication& other) {
    return plankton::SyntacticalEqual(*object.premise, *other.premise) &&
           plankton::SyntacticalEqual(*object.conclusion, *other.conclusion);
}

inline bool IsEqual(const PastPredicate& object, const PastPredicate& other) {
    return IsEqual(*object.formula, *other.formula);
}

inline bool IsEqual(const FuturePredicate& object, const FuturePredicate& other) {
    return &object.command == &other.command &&
           plankton::SyntacticalEqual(*object.pre, *other.pre) && plankton::SyntacticalEqual(*object.post, *other.post);
}

inline bool IsEqual(const Annotation& object, const Annotation& other) {
    if (object.past.size() != other.past.size()) return false;
    if (object.future.size() != other.future.size()) return false;
    if (!IsEqual(*object.now, *other.now)) return false;
    for (std::size_t index = 0; index < object.past.size(); ++index) {
        if (!IsEqual(*object.past.at(index), *other.past.at(index))) return false;
    }
    for (std::size_t index = 0; index < object.future.size(); ++index) {
        if (!IsEqual(*object.future.at(index), *other.future.at(index))) return false;
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
    void Visit(const SeparatingImplication& object) override { Compare(object); }
    void Visit(const PastPredicate& object) override { Compare(object); }
    void Visit(const FuturePredicate& object) override { Compare(object); }
    void Visit(const Annotation& object) override { Compare(object); }
};

bool plankton::SyntacticalEqual(const LogicObject& object, const LogicObject& other) {
    Comparator comparator(other);
    object.Accept(comparator);
    return comparator.result;
}
