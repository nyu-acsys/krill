#pragma once
#ifndef PLANKTON_LOGICS_UTIL_HPP
#define PLANKTON_LOGICS_UTIL_HPP

#include "ast.hpp"

namespace plankton {

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<LogicObject, T>>>
    std::unique_ptr<T> Copy(const T& object); // TODO: implement
    
    template<typename T>
    std::set<const T*> Collect(const LogicObject& object);

    std::string ToString(const LogicObject& object);
    std::string ToString(const SymbolDeclaration& object);
    std::string ToString(ObligationAxiom::Kind object);

    void Print(const LogicObject& object, std::ostream& out);

    bool SyntacticalEqual(const SymbolicExpression& object, const SymbolicExpression& other);
    bool SyntacticalEqual(const Formula& object, const Formula& other);

    void Simplify(LogicObject& object);
    void InlineAndSimplify(LogicObject& object);
    
    using SymbolRenaming = const std::function<const SymbolDeclaration&(const SymbolDeclaration&)>&;
    void RenameSymbolicSymbols(LogicObject& object, SymbolRenaming renaming);
    void RenameSymbolicSymbols(LogicObject& object, SymbolFactory& factory);
    void RenameSymbolicSymbols(LogicObject& object, const LogicObject& avoidSymbolsFrom);
//    void RenameSymbolicSymbols(const std::vector<LogicObject*>& objects, SymbolFactory& factory);

//    std::unique_ptr<SeparatingConjunction> Conjoin(std::unique_ptr<Formula> formula, std::unique_ptr<Formula> other);
//    std::unique_ptr<Annotation> Conjoin(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other);
    
} // namespace plankton

#endif //PLANKTON_LOGICS_UTIL_HPP
