#pragma once
#ifndef HEAL_SYMBOLIC
#define HEAL_SYMBOLIC

#include <set>
#include "cola/ast.hpp"
#include "logic.hpp"

namespace heal {

    struct SymbolicVariableSetComparator {
        bool operator() (const std::reference_wrapper<const cola::VariableDeclaration>& var, const std::reference_wrapper<const cola::VariableDeclaration>& other) const;
    };

    using SymbolicVariableSet = std::set<std::reference_wrapper<const cola::VariableDeclaration>, SymbolicVariableSetComparator>;


    bool IsSymbolicVariable(const cola::VariableDeclaration& decl);

    const cola::VariableDeclaration& MakeFreshSymbolicVariable(const cola::Type& type);

    const cola::VariableDeclaration& GetUnusedSymbolicVariable(const cola::Type& type, const SymbolicVariableSet& variablesInUse);

    const cola::VariableDeclaration& GetUnusedSymbolicVariable(const cola::Type& type, const LogicObject& object);

    SymbolicVariableSet CollectSymbolicVariables(const LogicObject& object);

    std::unique_ptr<Formula> RenameSymbolicVariables(std::unique_ptr<Formula> formula, const LogicObject& avoidSymbolicVariablesFrom);
    std::unique_ptr<TimePredicate> RenameSymbolicVariables(std::unique_ptr<TimePredicate> predicate, const LogicObject& avoidSymbolicVariablesFrom);
    std::unique_ptr<Annotation> RenameSymbolicVariables(std::unique_ptr<Annotation> annotation, const LogicObject& avoidSymbolicVariablesFrom);

}

#endif
