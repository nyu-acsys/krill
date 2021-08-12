#pragma once
#ifndef PLANKTON_ENGINE_ENCODING_HPP
#define PLANKTON_ENGINE_ENCODING_HPP

#include <z3++.h>
#include <map>
#include <variant>
#include "logics/ast.hpp"
#include "solver.hpp"
#include "flowgraph.hpp"

namespace plankton {
    
    struct EExpr {
        EExpr operator!() const;
        EExpr operator&&(const EExpr& other) const;
        EExpr operator||(const EExpr& other) const;
        EExpr operator==(const EExpr& other) const;
        EExpr operator!=(const EExpr& other) const;
        EExpr operator<(const EExpr& other) const;
        EExpr operator<=(const EExpr& other) const;
        EExpr operator>(const EExpr& other) const;
        EExpr operator>=(const EExpr& other) const;
        EExpr operator>>(const EExpr& other) const;
        EExpr operator()(const EExpr& other) const;
        
        private:
            std::variant<z3::expr, z3::func_decl> repr;
            explicit EExpr(const z3::expr& repr) : repr(repr) {}
            explicit EExpr(const z3::func_decl& repr) : repr(repr) {}
            
            // TODO: error handling?
            [[nodiscard]] inline z3::expr AsExpr() const { return std::get<z3::expr>(repr); }
            
            friend struct Encoding;
    };
    
    struct Encoding { // TODO: rename to 'StackEncoding' ?
        explicit Encoding();
        explicit Encoding(const Formula& premise);
        
        void AddPremise(const EExpr& expr);
        void AddPremise(const Formula& premise);
        void AddPremise(const SeparatingImplication& premise);
        void Push();
        void Pop();
        
        void AddCheck(const EExpr& expr, std::function<void(bool)> callback);
        void Check();
        
        bool ImpliesFalse();
        bool Implies(const EExpr& expr);
        bool Implies(const Formula& formula);
        bool Implies(const SeparatingImplication& formula);
        std::set<const SymbolDeclaration*> ComputeNonNull(std::set<const SymbolDeclaration*> symbols);
        
        EExpr Encode(const LogicObject& object);
        EExpr Encode(const VariableDeclaration& decl);
        EExpr Encode(const SymbolDeclaration& decl);
        
        EExpr EncodeExists(Sort sort, const std::function<EExpr(EExpr)>& makeInner);
        EExpr EncodeForAll(Sort sort, const std::function<EExpr(EExpr)>& makeInner);
        EExpr EncodeFlowContains(const SymbolDeclaration& flow, const EExpr& expr);
        EExpr EncodeIsNonNull(const EExpr& expr);
        EExpr EncodeIsNonNull(const SymbolDeclaration& decl);
        EExpr EncodeIsNonNull(const VariableDeclaration& decl);
        EExpr EncodeMemoryEquality(const MemoryAxiom& memory, const MemoryAxiom& other);
        EExpr EncodeInvariants(const Formula& formula, const SolverConfig& config);
        
        EExpr Min();
        EExpr Max();
        EExpr Null();
        EExpr Bool(bool val);
        
        EExpr MakeDistinct(const std::vector<EExpr>& expressions);
        EExpr MakeAnd(const std::vector<EExpr>& expressions);
        EExpr MakeOr(const std::vector<EExpr>& expressions);

        private:
            z3::context context;
            z3::solver solver;
            std::deque<z3::expr> checks_premise;
            std::deque<std::function<void(bool)>> checks_callback;
            std::map<const VariableDeclaration*, EExpr> variableEncoding;
            std::map<const SymbolDeclaration*, EExpr> symbolEncoding;
            
            z3::sort EncodeSort(Sort sort);
            z3::expr MakeQuantifiedVariable(Sort sort);
    };
    
} // plankton

#endif //PLANKTON_ENGINE_ENCODING_HPP
