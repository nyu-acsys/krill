#pragma once
#ifndef SOLVER_EVAL
#define SOLVER_EVAL

#include "cola/ast.hpp"
#include "heal/logic.hpp"

namespace solver {

    // TODO: FindResource that returns unique_ptr reference, FindResource for memory by SymbolicVariable

    heal::EqualsToAxiom* FindResource(const cola::VariableDeclaration& variable, heal::LogicObject& object);
    const heal::EqualsToAxiom* FindResource(const cola::VariableDeclaration& variable, const heal::LogicObject& object);

    heal::PointsToAxiom* FindResource(const cola::Dereference& dereference, heal::LogicObject& object);
    const heal::PointsToAxiom* FindResource(const cola::Dereference& dereference, const heal::LogicObject& object);

    template<bool LAZY>
    class ValuationMap final {
        private:
            const heal::LogicObject& context;
            std::map<const cola::VariableDeclaration*, const heal::SymbolicVariableDeclaration*> variableToSymbolic;
            std::map<std::pair<const cola::VariableDeclaration*, std::string>, const heal::SymbolicVariableDeclaration*> selectorToSymbolic;

        public: // TODO: [[nodiscard]]
            explicit ValuationMap(const heal::LogicObject& context) : context(context) {}

            const heal::PointsToAxiom* GetMemoryResourceOrNull(const heal::SymbolicVariableDeclaration& variable) { throw std::logic_error("not yet implemented"); }
            const heal::PointsToAxiom& GetMemoryResourceOrFail(const heal::SymbolicVariableDeclaration& variable) { throw std::logic_error("not yet implemented"); }

            const heal::SymbolicVariableDeclaration* GetValueOrNull(const cola::VariableDeclaration& variable);
            const heal::SymbolicVariableDeclaration& GetValueOrFail(const cola::VariableDeclaration& variable);

            const heal::SymbolicVariableDeclaration* GetValueOrNull(const cola::Dereference& dereference);
            const heal::SymbolicVariableDeclaration& GetValueOrFail(const cola::Dereference& dereference);

            std::unique_ptr<heal::SymbolicExpression> Evaluate(const cola::Expression& expr);
            std::unique_ptr<heal::SymbolicVariable> Evaluate(const cola::VariableExpression& expr);
            std::unique_ptr<heal::SymbolicVariable> Evaluate(const cola::Dereference& expr);
    };

    using LazyValuationMap = ValuationMap<true>;
    using EagerValuationMap = ValuationMap<false>;

}

#endif
