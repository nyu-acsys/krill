#include "logics/util.hpp"

#include <algorithm>

using namespace plankton;

constexpr std::size_t MAX_NUM_ITERATIONS = 6;


//
// Ordering: fast approximation based on class
//

struct IncidenceVisitor : public BaseLogicVisitor {
    std::size_t result = 0;
    void Visit(const SymbolicVariable& /*object*/) override { result = 110; }
    void Visit(const SymbolicBool& /*object*/) override { result = 120; }
    void Visit(const SymbolicNull& /*object*/) override { result = 130; }
    void Visit(const SymbolicMin& /*object*/) override { result = 140; }
    void Visit(const SymbolicMax& /*object*/) override { result = 150; }
    void Visit(const EqualsToAxiom& /*object*/) override { result = 210; }
    void Visit(const LocalMemoryResource& /*object*/) override { result = 220; }
    void Visit(const SharedMemoryCore& /*object*/) override { result = 230; }
    void Visit(const ObligationAxiom& /*object*/) override { result = 240; }
    void Visit(const FulfillmentAxiom& /*object*/) override { result = 250; }
    void Visit(const StackAxiom& /*object*/) override { result = 260; }
    void Visit(const InflowEmptinessAxiom& /*object*/) override { result = 270; }
    void Visit(const InflowContainsValueAxiom& /*object*/) override { result = 280; }
    void Visit(const InflowContainsRangeAxiom& /*object*/) override { result = 290; }
};

inline std::size_t GetIncidence(const LogicObject& object) {
    IncidenceVisitor visitor;
    object.Accept(visitor);
    return visitor.result;
}


//
// Ordering non-virtual expressions/axioms
//

bool LLessLogic(const LogicObject& object, const LogicObject& other);

bool LLess(const SymbolicVariable& object, const SymbolicVariable& other) {
    return &object < &other;
}

bool LLess(const SymbolicBool& object, const SymbolicBool& other) {
    return object.value < other.value;
}

bool LLess(const SymbolicNull& /*object*/, const SymbolicNull& /*other*/) {
    return false;
}

bool LLess(const SymbolicMin& /*object*/, const SymbolicMin& /*other*/) {
    return false;
}

bool LLess(const SymbolicMax& /*object*/, const SymbolicMax& /*other*/) {
    return false;
}

bool LLess(const MemoryAxiom& object, const MemoryAxiom& other) {
    return LLess(*object.node, *other.node) && LLess(*object.flow, *other.flow) &&
           plankton::All(object.fieldToValue, [&other](auto& pair) {
               return LLess(*pair.second, *other.fieldToValue.at(pair.first));
           });
}

bool LLess(const EqualsToAxiom& object, const EqualsToAxiom& other) {
    return &object.Variable() < & other.Variable() && LLess(*object.value, *other.value);
}

bool LLess(const StackAxiom& object, const StackAxiom& other) {
    return object.op < other.op && LLessLogic(*object.lhs, *other.lhs) && LLessLogic(*object.rhs, *other.rhs);
}

bool LLess(const InflowEmptinessAxiom& object, const InflowEmptinessAxiom& other) {
    return object.isEmpty < other.isEmpty && LLess(*object.flow, *other.flow);
}

bool LLess(const InflowContainsValueAxiom& object, const InflowContainsValueAxiom& other) {
    return LLess(*object.flow, *other.flow) && LLess(*object.value, *other.value);
}

bool LLess(const InflowContainsRangeAxiom& object, const InflowContainsRangeAxiom& other) {
    return LLess(*object.flow, *other.flow) &&
           LLessLogic(*object.valueLow, *other.valueLow) && LLessLogic(*object.valueHigh, *other.valueHigh);
}

bool LLess(const ObligationAxiom& object, const ObligationAxiom& other) {
    return object.spec < other.spec && LLess(*object.key, *other.key);
}

bool LLess(const FulfillmentAxiom& object, const FulfillmentAxiom& other) {
    return object.returnValue < other.returnValue;
}


//
// Ordering superclasses
//

struct LLessVisitor : public BaseLogicVisitor {
    const LogicObject& object;
    const LogicObject& other;
    bool result = false;
    
    LLessVisitor(const LogicObject& object, const LogicObject& other) : object(object), other(other) {}
    
    inline bool GetResult() {
        object.Accept(*this);
        return result;
    };
    
    template<typename T>
    void Handle(const T& obj) {
        assert(&object == &obj);
        if (auto otherCast = dynamic_cast<const T*>(&other)) {
            assert(&other == otherCast);
            result = LLess(obj, *otherCast);
            return;
        }
        throw std::logic_error("Internal error: cannot normalize."); // TODO: better error handling
    }
    
    void Visit(const SymbolicVariable& obj) override { Handle(obj); }
    void Visit(const SymbolicBool& obj) override { Handle(obj); }
    void Visit(const SymbolicNull& obj) override { Handle(obj); }
    void Visit(const SymbolicMin& obj) override { Handle(obj); }
    void Visit(const SymbolicMax& obj) override { Handle(obj); }
    void Visit(const LocalMemoryResource& obj) override { Handle(obj); }
    void Visit(const SharedMemoryCore& obj) override { Handle(obj); }
    void Visit(const EqualsToAxiom& obj) override { Handle(obj); }
    void Visit(const StackAxiom& obj) override { Handle(obj); }
    void Visit(const InflowEmptinessAxiom& obj) override { Handle(obj); }
    void Visit(const InflowContainsValueAxiom& obj) override { Handle(obj); }
    void Visit(const InflowContainsRangeAxiom& obj) override { Handle(obj); }
    void Visit(const ObligationAxiom& obj) override { Handle(obj); }
    void Visit(const FulfillmentAxiom& obj) override { Handle(obj); }
};

bool LLessLogic(const LogicObject& object, const LogicObject& other) {
    auto objectIn = GetIncidence(object);
    auto otherIn = GetIncidence(other);
    if (objectIn != otherIn) return objectIn < otherIn;
    LLessVisitor comparator(object, other);
    return comparator.GetResult();
}

bool LLessNow(const std::unique_ptr<Formula>& object, const std::unique_ptr<Formula>& other) {
    return LLessLogic(*object, *other);
}

bool LLessPast(const std::unique_ptr<PastPredicate>& object, const std::unique_ptr<PastPredicate>& other) {
    return LLessLogic(*object->formula, *other->formula);
}

bool LLessFuture(const std::unique_ptr<FuturePredicate>& object, const std::unique_ptr<FuturePredicate>& other) {
    return &object->command < &other->command &&
           LLessLogic(*object->pre, *other->pre) && LLessLogic(*object->post, *other->post);
}


//
// Normalization through sorting
//

inline void SwitchStackAxiom(StackAxiom& axiom) {
    axiom.op = plankton::Symmetric(axiom.op);
    auto tmp = std::move(axiom.lhs);
    axiom.lhs = std::move(axiom.rhs);
    axiom.rhs = std::move(tmp);
}

inline void NormalizeStackAxiom(StackAxiom& axiom) {
    switch (axiom.op) {
        case BinaryOperator::EQ:
        case BinaryOperator::NEQ:
            if (!LLessLogic(*axiom.lhs, *axiom.rhs))
                SwitchStackAxiom(axiom);
            break;
        
        case BinaryOperator::LEQ:
        case BinaryOperator::LT:
            break;
        
        case BinaryOperator::GEQ:
        case BinaryOperator::GT:
            SwitchStackAxiom(axiom);
            break;
    }
}

inline void NormalizeStackAxioms(Annotation& annotation) {
    auto normalizeWithin = [](LogicObject& object){
        auto stackAxioms = plankton::CollectMutable<StackAxiom>(object);
        for (auto* axiom : stackAxioms) NormalizeStackAxiom(*axiom);
    };
    normalizeWithin(*annotation.now);
    for (auto& past : annotation.past) normalizeWithin(*past);
    for (auto& future : annotation.future) normalizeWithin(*future);
}

inline void LSort(Annotation& annotation) {
    NormalizeStackAxioms(annotation);
    std::sort(annotation.now->conjuncts.begin(), annotation.now->conjuncts.end(), LLessNow);
    std::sort(annotation.past.begin(), annotation.past.end(), LLessPast);
    std::sort(annotation.future.begin(), annotation.future.end(), LLessFuture);
}

inline bool IsLSorted(const Annotation& annotation) {
    return std::is_sorted(annotation.now->conjuncts.begin(), annotation.now->conjuncts.end(), LLessNow) &&
           std::is_sorted(annotation.past.begin(), annotation.past.end(), LLessPast) &&
           std::is_sorted(annotation.future.begin(), annotation.future.end(), LLessFuture);
}

std::unique_ptr<Annotation> plankton::Normalize(std::unique_ptr<Annotation> annotation) {
    plankton::Simplify(*annotation);
    LSort(*annotation);
    for (std::size_t index = 0; !IsLSorted(*annotation) && index < MAX_NUM_ITERATIONS; ++index) {
        SymbolFactory factory;
        plankton::RenameSymbols(*annotation, factory);
        LSort(*annotation);
    }
    return annotation;
}
