#pragma once
#ifndef PLANKTON_ENGINE_UTIL_HPP
#define PLANKTON_ENGINE_UTIL_HPP

#include "programs/ast.hpp"
#include "logics//ast.hpp"

namespace plankton {
    // TODO: move functions to solver if they require a SolverConfig ?
    
    const EqualsToAxiom& GetResource(const VariableDeclaration& variable, const Formula& state);
    EqualsToAxiom& GetResource(const VariableDeclaration& variable, Formula& state);
    
    const MemoryAxiom& GetResource(const SymbolDeclaration& address, const Formula& state);
    MemoryAxiom& GetResource(const SymbolDeclaration& address, Formula& state);
    
    const SymbolDeclaration& Evaluate(const VariableExpression& variable, const Formula& state);
    const SymbolDeclaration& Evaluate(const Dereference& dereference, const Formula& state);
    
    std::unique_ptr<SymbolicExpression> MakeSymbolic(const ValueExpression& expression, const Formula& context);
    
    void MakeMemoryAccessible(Annotation& annotation, const Command& command);
    void CheckAccess(const Command& command, const Annotation& annotation);
    
    bool ImpliesFalse(const Formula& formula);
    
}

#endif //PLANKTON_ENGINE_UTIL_HPP
