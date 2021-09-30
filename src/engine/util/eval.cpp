#include "engine/util.hpp"

#include "logics/util.hpp"

using namespace plankton;


template<typename T, typename U>
const T* GetResourceOrNull(const Formula& state, const U& filter) {
    auto collect = plankton::Collect<T>(state, filter);
    if (collect.empty()) return nullptr;
    assert(collect.size() == 1); // TODO: this is wrong ~> there may be multiple SharedMemoryCores for one address
    return *collect.begin();
}

template<typename T, typename U>
const T& GetResourceOrFail(const Formula& state, const U& filter) {
    auto result = GetResourceOrNull<T>(state, filter);
    if (!result) throw std::logic_error("Internal error: cannot find resource"); // better error handling
    return *result;
}

const EqualsToAxiom* plankton::TryGetResource(const VariableDeclaration& variable, const Formula& state) {
    return GetResourceOrNull<EqualsToAxiom>(state, [&variable](auto& obj){ return obj.Variable() == variable; });
}

const EqualsToAxiom& plankton::GetResource(const VariableDeclaration& variable, const Formula& state) {
    return GetResourceOrFail<EqualsToAxiom>(state, [&variable](auto& obj){ return obj.Variable() == variable; });
}

EqualsToAxiom& plankton::GetResource(const VariableDeclaration& variable, Formula& state) {
    return const_cast<EqualsToAxiom&>(plankton::GetResource(variable, std::as_const(state)));
}

const MemoryAxiom* plankton::TryGetResource(const SymbolDeclaration& address, const Formula& state) {
    return GetResourceOrNull<MemoryAxiom>(state, [&address](auto& obj){ return obj.node->Decl() == address; });
}

const MemoryAxiom& plankton::GetResource(const SymbolDeclaration& address, const Formula& state) {
    return GetResourceOrFail<MemoryAxiom>(state, [&address](auto& obj){ return obj.node->Decl() == address; });
}

MemoryAxiom& plankton::GetResource(const SymbolDeclaration& address, Formula& state) {
    return const_cast<MemoryAxiom&>(plankton::GetResource(address, std::as_const(state)));
}

const SymbolDeclaration& plankton::Evaluate(const VariableExpression& variable, const Formula& state) {
    auto& resource = plankton::GetResource(variable.Decl(), state);
    return resource.value->Decl();
}

const SymbolDeclaration& plankton::Evaluate(const Dereference& dereference, const Formula& state) {
    auto& address = plankton::Evaluate(*dereference.variable, state);
    auto& memory = plankton::GetResource(address, state);
    return memory.fieldToValue.at(dereference.fieldName)->Decl();
}
