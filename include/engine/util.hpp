#pragma once
#ifndef PLANKTON_ENGINE_UTIL_HPP
#define PLANKTON_ENGINE_UTIL_HPP

#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/solver.hpp"
#include "engine/encoding.hpp"

namespace plankton {
    
    const EqualsToAxiom* TryGetResource(const VariableDeclaration& variable, const Formula& state);
    const EqualsToAxiom& GetResource(const VariableDeclaration& variable, const Formula& state);
    EqualsToAxiom& GetResource(const VariableDeclaration& variable, Formula& state);
    
    const MemoryAxiom* TryGetResource(const SymbolDeclaration& address, const Formula& state);
    const MemoryAxiom& GetResource(const SymbolDeclaration& address, const Formula& state);
    MemoryAxiom& GetResource(const SymbolDeclaration& address, Formula& state);
    
    const SymbolDeclaration& Evaluate(const VariableExpression& variable, const Formula& state);
    const SymbolDeclaration& Evaluate(const Dereference& dereference, const Formula& state);
    
    std::unique_ptr<SymbolicExpression> MakeSymbolic(const ValueExpression& expression, const Formula& context);
    
    bool UpdatesFlow(const HeapEffect& effect);
    bool UpdatesField(const HeapEffect& effect, const std::string& field);
    
    void AvoidEffectSymbols(SymbolFactory& factory, const HeapEffect& effect);
    void AvoidEffectSymbols(SymbolFactory& factory, const std::deque<std::unique_ptr<HeapEffect>>& effects);
    
    void MakeMemoryAccessible(SeparatingConjunction& formula, std::set<const SymbolDeclaration*> addresses,
                              const Type& flowType, SymbolFactory& factory, Encoding& encoding);
    void MakeMemoryAccessible(Annotation& annotation, std::set<const SymbolDeclaration*> addresses,
                              const Type& flowType);
    
    enum struct ExtensionPolicy { POINTERS, FAST, FULL };
    void ExtendStack(Annotation& annotation, Encoding& encoding, ExtensionPolicy policy);
    void ExtendStack(Annotation& annotation, ExtensionPolicy policy);

}

#endif //PLANKTON_ENGINE_UTIL_HPP
