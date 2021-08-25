#include "programs/ast.hpp"

#include <cassert>
#include "programs/util.hpp"

using namespace plankton;


//
// Types, Variables
//

Type::Type(std::string name, Sort sort) : name(std::move(name)), sort(sort) {}

decltype(Type::fields)::const_iterator Type::begin() const { return fields.cbegin(); }

decltype(Type::fields)::const_iterator Type::end() const { return fields.cend(); }

bool Type::operator==(const Type& other) const { return this == &other; }

bool Type::operator!=(const Type& other) const { return this != &other; }

const Type& Type::operator[](const std::string& fieldName) const { return fields.at(fieldName); }

bool Type::AssignableFrom(const Type& other) const { return other.AssignableTo(*this); }

bool Type::Comparable(const Type& other) const { return AssignableTo(other) || AssignableFrom(other); }

const Type& Type::Bool() {
    static const Type type = Type("bool", Sort::BOOL);
    return type;
}

const Type& Type::Data() {
    static const Type type = Type("data_t", Sort::DATA);
    return type;
}

std::optional<std::reference_wrapper<const Type>> Type::GetField(const std::string& fieldName) const {
    auto find = fields.find(fieldName);
    if (find == fields.end()) return std::nullopt;
    else return find->second;
}

bool Type::AssignableTo(const Type& other) const {
    if (sort != Sort::PTR) return sort == other.sort;
    else return *this == other;
}

VariableDeclaration::VariableDeclaration(std::string name, const Type& type, bool shared)
: name(std::move(name)), type(type), isShared(shared) {}

bool VariableDeclaration::operator==(const VariableDeclaration& other) const { return this == &other; }

bool VariableDeclaration::operator!=(const VariableDeclaration& other) const { return this != &other; }


//
// Programs, Functions
//

Program::Program(std::string name, std::unique_ptr<Function> init)
: name(std::move(name)), initializer(std::move(init)) {
    assert(initializer);
}

Function::Function(std::string name, Kind kind, std::unique_ptr<Scope> bdy)
        : name(std::move(name)), kind(kind), body(std::move(bdy)) {
    assert(body);
}


//
// Expressions
//

VariableExpression::VariableExpression(const VariableDeclaration& decl) : decl(decl) {}

const Type& VariableExpression::Type() const { return Decl().type; }

const plankton::Type& TrueValue::Type() const { return plankton::Type::Bool(); }

const plankton::Type& FalseValue::Type() const { return plankton::Type::Bool(); }

const Type& MinValue::Type() const { return plankton::Type::Data(); }

const Type& MaxValue::Type() const { return plankton::Type::Data(); }

NullValue::NullValue(const plankton::Type& type) : type(type) {
    assert(type.sort == Sort::PTR); // TODO: throw
}

const Type& NullValue::Type() const { return type; }

Dereference::Dereference(std::unique_ptr<VariableExpression> var, std::string fieldName)
        : variable(std::move(var)), fieldName(std::move(fieldName)) {
    assert(variable->Sort() == Sort::PTR); // TODO: throw
    assert(variable->Type().GetField(fieldName).has_value()); // TODO: throw
}

const Type& Dereference::Type() const {
    return variable->Type()[fieldName];
}

BinaryOperator plankton::Symmetric(BinaryOperator op) {
    switch (op) {
        case BinaryOperator::EQ: return BinaryOperator::EQ;
        case BinaryOperator::NEQ: return BinaryOperator::NEQ;
        case BinaryOperator::LEQ: return BinaryOperator::GEQ;
        case BinaryOperator::LT: return BinaryOperator::GT;
        case BinaryOperator::GEQ: return BinaryOperator::LEQ;
        case BinaryOperator::GT: return BinaryOperator::LT;
    }
}

BinaryOperator plankton::Negate(BinaryOperator op) {
    switch (op) {
        case BinaryOperator::EQ: return BinaryOperator::NEQ;
        case BinaryOperator::NEQ: return BinaryOperator::EQ;
        case BinaryOperator::LEQ: return BinaryOperator::GT;
        case BinaryOperator::LT: return BinaryOperator::GEQ;
        case BinaryOperator::GEQ: return BinaryOperator::LT;
        case BinaryOperator::GT: return BinaryOperator::LEQ;
    }
}

BinaryExpression::BinaryExpression(BinaryOperator op, std::unique_ptr<ValueExpression> lhs_,
                                   std::unique_ptr<ValueExpression> rhs_)
        : op(op), lhs(std::move(lhs_)), rhs(std::move(rhs_)) {
    assert(lhs);
    assert(rhs);
    assert(lhs->Type().Comparable(rhs->Type()));
    assert(op == BinaryOperator::EQ || op == BinaryOperator::NEQ || lhs->Type() == Type::Data());
}

const Type& BinaryExpression::Type() const { return plankton::Type::Bool(); }


//
// Statements
//

Scope::Scope(std::unique_ptr<Statement> bdy) : body(std::move(bdy)) {
    assert(body);
}

Atomic::Atomic(std::unique_ptr<Scope>  bdy) : body(std::move(bdy)) {
    assert(body);
}

Sequence::Sequence(std::unique_ptr<Statement> st, std::unique_ptr<Statement> nd)
        : first(std::move(st)), second(std::move(nd)) {
    assert(first);
    assert(second);
}

UnconditionalLoop::UnconditionalLoop(std::unique_ptr<Scope> bdy) : body(std::move(bdy)) {
    assert(body);
}

Choice::Choice() = default;

Choice::Choice(std::unique_ptr<Scope> branch1, std::unique_ptr<Scope> branch2) {
    assert(branch1);
    assert(branch2);
    branches.push_back(std::move(branch1));
    branches.push_back(std::move(branch2));
}


//
// Commands
//

Skip::Skip() = default;

Fail::Fail() = default;

Break::Break() = default;

Return::Return() = default;

Return::Return(std::unique_ptr<SimpleExpression> expression) {
    assert(expression);
    expressions.push_back(std::move(expression));
}

Assume::Assume(std::unique_ptr<BinaryExpression> cond) : condition(std::move(cond)) {
    assert(condition);
    assert(condition->Type() == Type::Bool());
}

Malloc::Malloc(std::unique_ptr<VariableExpression> left) : lhs(std::move(left)) {
    assert(lhs);
}

Macro::Macro(const Function& function) : function(function) {
}

const Function& Macro::Func() const { return function; }

template<typename L, typename R>
Assignment<L,R>::Assignment() = default;

template<typename L, typename R>
Assignment<L,R>::Assignment(std::unique_ptr<L> left, std::unique_ptr<R> right) {
    assert(left);
    assert(right);
    lhs.push_back(std::move(left));
    rhs.push_back(std::move(right));
}


//
// Output
//

std::ostream& plankton::operator<<(std::ostream& stream, const Sort& object) {
    switch (object) {
        case Sort::VOID: stream << "VoidSort"; break;
        case Sort::BOOL: stream << "BoolSort"; break;
        case Sort::DATA: stream << "DataSort"; break;
        case Sort::PTR: stream << "PointerSort"; break;
    }
    return stream;
}

std::ostream& plankton::operator<<(std::ostream& stream, const BinaryOperator& object) {
    switch (object) {
        case BinaryOperator::EQ: stream << "=="; break;
        case BinaryOperator::NEQ: stream << "!="; break;
        case BinaryOperator::LEQ: stream << "<="; break;
        case BinaryOperator::LT: stream << "<"; break;
        case BinaryOperator::GEQ: stream << ">="; break;
        case BinaryOperator::GT: stream << ">"; break;
    }
    return stream;
}

std::ostream& plankton::operator<<(std::ostream& stream, const Type& object) {
    stream << "type " << object.name << " { ";
    bool first = true;
    for (const auto& [field, type] : object) {
        if (first) first = false;
        else stream << ", ";
        stream << field << ":" << type.get().name;
    }
    stream << " }";
    return stream;
}

std::ostream& plankton::operator<<(std::ostream& stream, const AstNode& object) {
    plankton::Print(object, stream);
    return stream;
}

std::ostream& plankton::operator<<(std::ostream& stream, const VariableDeclaration& object) {
    stream << object.type.name << " " << object.name << ";";
    return stream;
}