#pragma once
#ifndef PLANKTON_LOGICS_UTIL_HPP
#define PLANKTON_LOGICS_UTIL_HPP

#include "ast.hpp"
#include "programs/util.hpp"
#include "util/shortcuts.hpp"

namespace plankton {

    std::string ToString(const LogicObject& object);
    std::string ToString(const SymbolDeclaration& object);
    std::string ToString(Specification object);

    void Print(const LogicObject& object, std::ostream& out);

    template<typename T, EnableIfBaseOf<LogicObject, T>>
    std::unique_ptr<T> Copy(const T& object);
  
   
    template<typename T>
    std::set<const T*> Collect(const LogicObject& object,
                               const std::function<bool(const T&)>& filter = [](auto&){ return true; });
    template<typename T>
    std::set<T*> CollectMutable(LogicObject& object,
                                const std::function<bool(const T&)>& filter = [](auto&) { return true; });

    bool SyntacticalEqual(const LogicObject& object, const LogicObject& other);
    std::unique_ptr<Annotation> Normalize(std::unique_ptr<Annotation> annotation);

    void Simplify(LogicObject& object);
    void InlineAndSimplify(LogicObject& object);
    
    using SymbolRenaming = std::function<const SymbolDeclaration&(const SymbolDeclaration&)>;
    SymbolRenaming MakeDefaultRenaming(SymbolFactory& factory);
    void RenameSymbols(LogicObject& object, const SymbolRenaming& renaming);
    void RenameSymbols(LogicObject& object, SymbolFactory& factory);
    void RenameSymbols(LogicObject& object, const LogicObject& avoidSymbolsFrom);
//    void RenameSymbolicSymbols(const std::vector<LogicObject*>& objects, SymbolFactory& factory);
    
    std::unique_ptr<SharedMemoryCore> MakeSharedMemory(const SymbolDeclaration& address, const Type& flowType, SymbolFactory& factory);
    std::unique_ptr<LocalMemoryResource> MakeLocalMemory(const SymbolDeclaration& address, const Type& flowType, SymbolFactory& factory);
    
    bool IsLocal(const MemoryAxiom& axiom);
    
} // namespace plankton

#endif //PLANKTON_LOGICS_UTIL_HPP
