#include "programs/util.hpp"

using namespace plankton;


//
// Handling superclasses
//

template<typename T>
struct CopyVisitor : public BaseProgramVisitor {
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
    
    void Visit(const VariableExpression& object) override { Handle(object); }
    void Visit(const TrueValue& object) override { Handle(object); }
    void Visit(const FalseValue& object) override { Handle(object); }
    void Visit(const MinValue& object) override { Handle(object); }
    void Visit(const MaxValue& object) override { Handle(object); }
    void Visit(const NullValue& object) override { Handle(object); }
    void Visit(const Dereference& object) override { Handle(object); }
    void Visit(const BinaryExpression& object) override { Handle(object); }
    void Visit(const Skip& object) override { Handle(object); }
    void Visit(const Break& object) override { Handle(object); }
    void Visit(const Return& object) override { Handle(object); }
    void Visit(const Assume& object) override { Handle(object); }
    void Visit(const Fail& object) override { Handle(object); }
    void Visit(const VariableAssignment& object) override { Handle(object); }
    void Visit(const MemoryWrite& object) override { Handle(object); }
};

template<>
std::unique_ptr<Expression> plankton::Copy<Expression>(const Expression& object) {
    return CopyVisitor<Expression>::Copy(object);
}

template<>
std::unique_ptr<ValueExpression> plankton::Copy<ValueExpression>(const ValueExpression& object) {
    return CopyVisitor<ValueExpression>::Copy(object);
}

template<>
std::unique_ptr<SimpleExpression> plankton::Copy<SimpleExpression>(const SimpleExpression& object) {
    return CopyVisitor<SimpleExpression>::Copy(object);
}

template<>
std::unique_ptr<Command> plankton::Copy<Command>(const Command& object) {
    return CopyVisitor<Command>::Copy(object);
}

//
// Copying objects
//

template<>
std::unique_ptr<VariableExpression> plankton::Copy<VariableExpression>(const VariableExpression& object) {
    return std::make_unique<VariableExpression>(object.decl);
}

template<>
std::unique_ptr<TrueValue> plankton::Copy<TrueValue>(const TrueValue& /*object*/) {
    return std::make_unique<TrueValue>();
}

template<>
std::unique_ptr<FalseValue> plankton::Copy<FalseValue>(const FalseValue& /*object*/) {
    return std::make_unique<FalseValue>();
}

template<>
std::unique_ptr<MinValue> plankton::Copy<MinValue>(const MinValue& /*object*/) {
    return std::make_unique<MinValue>();
}

template<>
std::unique_ptr<MaxValue> plankton::Copy<MaxValue>(const MaxValue& /*object*/) {
    return std::make_unique<MaxValue>();
}

template<>
std::unique_ptr<NullValue> plankton::Copy<NullValue>(const NullValue& object) {
    return std::make_unique<NullValue>(object.type);
}

template<>
std::unique_ptr<Dereference> plankton::Copy<Dereference>(const Dereference& object) {
    return std::make_unique<Dereference>(std::make_unique<VariableExpression>(object.variable->decl), object.fieldName);
}

template<>
std::unique_ptr<BinaryExpression> plankton::Copy<BinaryExpression>(const BinaryExpression& object) {
    return std::make_unique<BinaryExpression>(object.op, plankton::Copy(*object.lhs), plankton::Copy(*object.rhs));
}

template<>
std::unique_ptr<Skip> plankton::Copy<Skip>(const Skip& /*object*/) {
    return std::make_unique<Skip>();
}

template<>
std::unique_ptr<Break> plankton::Copy<Break>(const Break& /*object*/) {
    return std::make_unique<Break>();
}

template<>
std::unique_ptr<Return> plankton::Copy<Return>(const Return& object) {
    auto result = std::make_unique<Return>();
    for (const auto& expr : object.expressions) {
        result->expressions.push_back(plankton::Copy(*expr));
    }
    return result;
}

template<>
std::unique_ptr<Assume> plankton::Copy<Assume>(const Assume& object) {
    return std::make_unique<Assume>(plankton::Copy(*object.condition));
}

template<>
std::unique_ptr<Fail> plankton::Copy<Fail>(const Fail& /*object*/) {
    return std::make_unique<Fail>();
}

template<>
std::unique_ptr<Malloc> plankton::Copy<Malloc>(const Malloc& object) {
    return std::make_unique<Malloc>(plankton::Copy(*object.lhs));
}

template<>
std::unique_ptr<Macro> plankton::Copy<Macro>(const Macro& object) {
    auto result = std::make_unique<Macro>(object.function);
    for (const auto& elem : object.lhs) result->lhs.push_back(plankton::Copy(*elem));
    for (const auto& elem : object.arguments) result->arguments.push_back(plankton::Copy(*elem));
    return result;
}

template<typename T>
std::unique_ptr<T> CopyAssignment(const T& assignment) {
    auto result = std::make_unique<T>();
    for (const auto& lhs : assignment.lhs) {
        result->lhs.push_back(plankton::Copy(*lhs));
    }
    for (const auto& rhs : assignment.rhs) {
        result->rhs.push_back(plankton::Copy(*rhs));
    }
    return result;
}

template<>
std::unique_ptr<VariableAssignment> plankton::Copy<VariableAssignment>(const VariableAssignment& object) {
    return CopyAssignment(object);
}

template<>
std::unique_ptr<MemoryWrite> plankton::Copy<MemoryWrite>(const MemoryWrite& object) {
    return CopyAssignment(object);
}
