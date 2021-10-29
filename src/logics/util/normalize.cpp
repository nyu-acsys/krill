#include "logics/util.hpp"

#include <algorithm>

using namespace plankton;

constexpr std::size_t MAX_NUM_ITERATIONS = 12; // 6


//
// Ordering: fast approximation based on class
//

struct IncidenceVisitor : public BaseLogicVisitor, public BaseProgramVisitor {
    using BaseLogicVisitor::Visit;
    using BaseProgramVisitor::Visit;
    std::size_t result = 0;
    void Visit(const VariableExpression& /*object*/) override { result = 010; }
    void Visit(const TrueValue& /*object*/) override { result = 20; }
    void Visit(const FalseValue& /*object*/) override { result = 30; }
    void Visit(const MinValue& /*object*/) override { result = 40; }
    void Visit(const MaxValue& /*object*/) override { result = 50; }
    void Visit(const NullValue& /*object*/) override { result = 60; }
    void Visit(const Dereference& /*object*/) override { result = 70; }
    void Visit(const BinaryExpression& /*object*/) override { result = 80; }
    void Visit(const SymbolicVariable& /*object*/) override { result = 110; }
    void Visit(const SymbolicBool& /*object*/) override { result = 120; }
    void Visit(const SymbolicNull& /*object*/) override { result = 130; }
    void Visit(const SymbolicMin& /*object*/) override { result = 140; }
    void Visit(const SymbolicMax& /*object*/) override { result = 150; }
    void Visit(const Guard& /*object*/) override { result = 160; }
    void Visit(const Update& /*object*/) override { result = 170; }
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

template<typename T>
inline std::size_t GetIncidence(const T& object) {
    IncidenceVisitor visitor;
    object.Accept(visitor);
    return visitor.result;
}


//
// Ordering non-virtual expressions/axioms
//

static struct SymbolOrder {
    std::map<const SymbolDeclaration*, std::size_t> symbolToIndex;

    inline std::size_t Get(const SymbolDeclaration& decl) {
        auto find = symbolToIndex.find(&decl);
        if (find == symbolToIndex.end()) find = symbolToIndex.emplace(&decl, symbolToIndex.size()).first;
        return find->second;
    }
} ordering;


inline bool LLessLogic(const LogicObject& object, const LogicObject& other);
inline bool LLessProgram(const Expression& object, const Expression& other);

inline bool LNeq(const VariableDeclaration& decl, const VariableDeclaration& other) {
    return &decl != &other;
}

inline bool LNeq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
    return &decl != &other;
}

inline bool LNeq(const SymbolicVariable& object, const SymbolicVariable& other) {
    return LNeq(object.Decl(), other.Decl());
}

inline bool LLess(const VariableDeclaration& decl, const VariableDeclaration& other) {
    return &decl < &other;
}

inline bool LLess(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
    // return &decl < &other;
    return ordering.Get(decl) < ordering.Get(other);
}

inline bool LLess(const VariableExpression& object, const VariableExpression& other) {
    return LLess(object.Decl(), other.Decl());
}

inline bool LLess(const TrueValue& /*object*/, const TrueValue& /*other*/) {
    return false;
}

inline bool LLess(const FalseValue& /*object*/, const FalseValue& /*other*/) {
    return false;
}

inline bool LLess(const MinValue& /*object*/, const MinValue& /*other*/) {
    return false;
}

inline bool LLess(const MaxValue& /*object*/, const MaxValue& /*other*/) {
    return false;
}

inline bool LLess(const NullValue& /*object*/, const NullValue& /*other*/) {
    return false;
}

inline bool LLess(const Dereference& object, const Dereference& other) {
    if (LLess(*object.variable, *other.variable)) return true;
    if (LLess(*other.variable, *object.variable)) return false;
    return object.fieldName < other.fieldName;
}

inline bool LLess(const BinaryExpression& object, const BinaryExpression& other) {
    if (object.op != other.op) return object.op < other.op;
    if (LLessProgram(*object.lhs, *other.lhs)) return true;
    if (LLessProgram(*other.lhs, *object.lhs)) return false;
    return LLessProgram(*other.rhs, *object.rhs);
}

inline bool LLess(const SymbolicVariable& object, const SymbolicVariable& other) {
    return LLess(object.Decl(), other.Decl());
}

inline bool LLess(const SymbolicBool& object, const SymbolicBool& other) {
    return object.value < other.value;
}

inline bool LLess(const SymbolicNull& /*object*/, const SymbolicNull& /*other*/) {
    return false;
}

inline bool LLess(const SymbolicMin& /*object*/, const SymbolicMin& /*other*/) {
    return false;
}

inline bool LLess(const SymbolicMax& /*object*/, const SymbolicMax& /*other*/) {
    return false;
}

inline bool LLess(const Guard& object, const Guard& other) {
    if (object.conjuncts.size() != other.conjuncts.size()) return object.conjuncts.size() < other.conjuncts.size();
    for (std::size_t index = 0; index < object.conjuncts.size(); ++index) {
        if (LLessProgram(*object.conjuncts.at(index), *other.conjuncts.at(index))) return true;
        if (LLessProgram(*other.conjuncts.at(index), *object.conjuncts.at(index))) return false;
    }
    return false;
}

inline bool LLess(const Update& object, const Update& other) {
    if (object.fields.size() != other.fields.size()) return object.fields.size() < other.fields.size();
    for (std::size_t index = 0; index < object.fields.size(); ++index) {
        if (LLessProgram(*object.fields.at(index), *other.fields.at(index))) return true;
        if (LLessProgram(*other.fields.at(index), *object.fields.at(index))) return false;
        if (LLessLogic(*object.values.at(index), *other.values.at(index))) return true;
        if (LLessLogic(*other.values.at(index), *object.values.at(index))) return false;
    }
    return false;
}

inline bool LLess(const MemoryAxiom& object, const MemoryAxiom& other) {
    if (LNeq(*object.node, *other.node)) return LLess(*object.node, *other.node);
    if (LNeq(*object.flow, *other.flow)) return LLess(*object.flow, *other.flow);
    for (const auto& [field, value] : object.fieldToValue) {
        auto& otherValue = *other.fieldToValue.at(field);
        if (LNeq(*value, otherValue)) return LLess(*value, otherValue);
    }
    return false;
}

inline bool LLess(const EqualsToAxiom& object, const EqualsToAxiom& other) {
    if (LNeq(object.Variable(), other.Variable())) return LLess(object.Variable(), other.Variable());
    return LLess(*object.value, *other.value);
}

inline bool LLess(const StackAxiom& object, const StackAxiom& other) {
    if (object.op != other.op) return object.op < other.op;
    if (LLessLogic(*object.lhs, *other.lhs)) return true;
    if (LLessLogic(*other.lhs, *object.lhs)) return false;
    return LLessLogic(*object.rhs, *other.rhs);
}

inline bool LLess(const InflowEmptinessAxiom& object, const InflowEmptinessAxiom& other) {
    if (object.isEmpty == other.isEmpty) return LLess(*object.flow, *other.flow);
    return object.isEmpty < other.isEmpty;
}

inline bool LLess(const InflowContainsValueAxiom& object, const InflowContainsValueAxiom& other) {
    if (LNeq(*object.flow, *other.flow)) return LLess(*object.flow, *other.flow);
    return LLess(*object.value, *other.value);
}

inline bool LLess(const InflowContainsRangeAxiom& object, const InflowContainsRangeAxiom& other) {
    if (LNeq(*object.flow, *other.flow)) return LLess(*object.flow, *other.flow);
    if (LLessLogic(*object.valueLow, *other.valueLow)) return true;
    if (LLessLogic(*other.valueLow, *object.valueLow)) return false;
    return LLessLogic(*object.valueHigh, *other.valueHigh);
}

inline bool LLess(const ObligationAxiom& object, const ObligationAxiom& other) {
    if (object.spec == other.spec) return LLess(*object.key, *other.key);
    return object.spec < other.spec;
}

inline bool LLess(const FulfillmentAxiom& object, const FulfillmentAxiom& other) {
    return object.returnValue < other.returnValue;
}


//
// Ordering superclasses
//

template<typename T>
struct LLessVisitor : public BaseLogicVisitor, public BaseProgramVisitor {
    using BaseLogicVisitor::Visit;
    using BaseProgramVisitor::Visit;

    const T& object;
    const T& other;
    bool result = false;

    LLessVisitor(const T& object, const T& other) : object(object), other(other) {}
    
    inline bool GetResult() {
        object.Accept(*this);
        return result;
    };
    
    template<typename U>
    void Handle(const U& obj) {
        if constexpr (std::is_base_of_v<T, U>) {
            assert(&object == &obj);
            if (auto otherCast = dynamic_cast<const U*>(&other)) {
                assert(&other == otherCast);
                result = LLess(obj, *otherCast);
                return;
            }
        }
        throw std::logic_error("Internal error: cannot normalize."); // TODO: better error handling
    }

    void Visit(const VariableExpression& obj) override { Handle(obj); }
    void Visit(const TrueValue& obj) override { Handle(obj); }
    void Visit(const FalseValue& obj) override { Handle(obj); }
    void Visit(const MinValue& obj) override { Handle(obj); }
    void Visit(const MaxValue& obj) override { Handle(obj); }
    void Visit(const NullValue& obj) override { Handle(obj); }
    void Visit(const Dereference& obj) override { Handle(obj); }
    void Visit(const BinaryExpression& obj) override { Handle(obj); }
    void Visit(const SymbolicVariable& obj) override { Handle(obj); }
    void Visit(const SymbolicBool& obj) override { Handle(obj); }
    void Visit(const SymbolicNull& obj) override { Handle(obj); }
    void Visit(const SymbolicMin& obj) override { Handle(obj); }
    void Visit(const SymbolicMax& obj) override { Handle(obj); }
    void Visit(const Guard& obj) override { Handle(obj); }
    void Visit(const Update& obj) override { Handle(obj); }
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

bool LLessProgram(const Expression& object, const Expression& other) {
    auto objectIn = GetIncidence(object);
    auto otherIn = GetIncidence(other);
    if (objectIn != otherIn) return objectIn < otherIn;
    LLessVisitor comparator(object, other);
    return comparator.GetResult();
}

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
    return LLessLogic(*object->guard, *other->guard) && LLessLogic(*object->update, *other->update);
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
            if (LLessLogic(*axiom.rhs, *axiom.lhs))
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

inline void ApplyRenaming(Annotation& annotation) {
    struct Collector : public LogicListener {
        SymbolFactory factory;
        SymbolRenaming renaming;
        Collector() : renaming(plankton::MakeDefaultRenaming(factory)) {}
        void Visit(const StackAxiom&) override { /* do nothing */ }
        void Visit(const InflowEmptinessAxiom&) override { /* do nothing */ }
        void Visit(const InflowContainsValueAxiom&) override { /* do nothing */ }
        void Visit(const InflowContainsRangeAxiom&) override { /* do nothing */ }
        void Enter(const SymbolDeclaration& object) override {
            ordering.Get(renaming(object));
            // (void) renaming(object);
        }
    } collector;
    annotation.Accept(collector);
    plankton::RenameSymbols(annotation, collector.renaming);
}

std::unique_ptr<Annotation> plankton::Normalize(std::unique_ptr<Annotation> annotation) {
    plankton::InlineAndSimplify(*annotation);
    std::size_t index = 0;
    do {
        LSort(*annotation);
        ApplyRenaming(*annotation);
    } while (!IsLSorted(*annotation) && index++ < MAX_NUM_ITERATIONS);
    return annotation;
}
