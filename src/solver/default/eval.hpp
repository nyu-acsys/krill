#pragma once
#ifndef SOLVER_EVAL
#define SOLVER_EVAL

#include "cola/ast.hpp"
#include "heal/logic.hpp"

namespace solver {

    heal::EqualsToAxiom* FindResource(const cola::VariableDeclaration& variable, heal::LogicObject& object);
    const heal::EqualsToAxiom* FindResource(const cola::VariableDeclaration& variable, const heal::LogicObject& object);

    heal::PointsToAxiom* FindResource(const cola::Dereference& dereference, heal::LogicObject& object);
    const heal::PointsToAxiom* FindResource(const cola::Dereference& dereference, const heal::LogicObject& object);

    template<bool LAZY>
    struct ValuationMap final {
        const heal::LogicObject& context;
        std::map<const cola::VariableDeclaration*, const heal::SymbolicVariableDeclaration*> variableToSymbolic;
        std::map<std::pair<const cola::VariableDeclaration*, std::string>, const heal::SymbolicVariableDeclaration*> selectorToSymbolic;

        explicit ValuationMap(const heal::LogicObject& context) : context(context) {}

        const heal::SymbolicVariableDeclaration* GetValueOrNull(const cola::VariableDeclaration& variable);
        const heal::SymbolicVariableDeclaration& GetValueOrFail(const cola::VariableDeclaration& variable);

        const heal::SymbolicVariableDeclaration* GetValueOrNull(const cola::Dereference& dereference);
        const heal::SymbolicVariableDeclaration& GetValueOrFail(const cola::Dereference& dereference);
    };

    using LazyValuationMap = ValuationMap<true>;
    using EagerValuationMap = ValuationMap<false>;

}

#endif
