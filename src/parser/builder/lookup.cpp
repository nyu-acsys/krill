#include "parser/builder.hpp"

#include "util/shortcuts.hpp"

using namespace plankton;

template<typename T>
inline const T* GetOrNull(const std::deque<std::unique_ptr<T>>& container, const std::string& name) {
    auto find = plankton::FindIf(container, [&name](auto& elem){ return elem->name == name; });
    if (find != container.end()) return find->get();
    return nullptr;
}

template<typename T>
inline const T& CheckResult(const T* object, const std::string& name) {
    if (object) return *object;
    throw std::logic_error("Parse error: undefined symbol '" + name + "'."); //  TODO: better error handling
}

const Type& AstBuilder::TypeByName(const std::string& name) {
    return CheckResult(TypeByNameOrNull(name), name);
}

const VariableDeclaration& AstBuilder::VariableByName(const std::string& name) {
    return CheckResult(VariableByNameOrNull(name), name);
}

const Function& AstBuilder::FunctionByName(const std::string& name) {
    return CheckResult(FunctionByNameOrNull(name), name);
}

const Type* AstBuilder::TypeByNameOrNull(const std::string& name) {
    if (name == "void") throw std::logic_error("Internal error: 'void' not allowed here."); // TODO: better error handling
    if (name == Type::Bool().name) return &Type::Bool();
    if (name == Type::Data().name) return &Type::Data();
    return GetOrNull(_types, name);
}

const Function* AstBuilder::FunctionByNameOrNull(const std::string& name) {
    if (name == "__init__") throw std::logic_error("Internal error: '__init__' not allowed here."); // TODO: better error handling
    return GetOrNull(_functions, name);
}

const VariableDeclaration* AstBuilder::VariableByNameOrNull(const std::string& name) {
    for (const auto& level : _variables) {
        if (auto find = GetOrNull(level, name)) return find;
    }
    return nullptr;
}

void AstBuilder::AddDecl(std::unique_ptr<Type> type)  {
    _types.push_back(std::move(type));
}

void AstBuilder::AddDecl(std::unique_ptr<VariableDeclaration> variable) {
    assert(!_variables.empty());
    _variables.back().push_back(std::move(variable));
}

void AstBuilder::AddDecl(std::unique_ptr<Function> function) {
    _functions.push_back(std::move(function));
}

void AstBuilder::PushScope() {
    _variables.emplace_back();
}

std::vector<std::unique_ptr<VariableDeclaration>> AstBuilder::PopScope() {
    auto back = std::move(_variables.back());
    _variables.pop_back();
    std::vector<std::unique_ptr<VariableDeclaration>> result;
    plankton::MoveInto(std::move(back), result);
    return result;
}
