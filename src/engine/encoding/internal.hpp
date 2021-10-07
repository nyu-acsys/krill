#pragma once
#ifndef PLANKTON_ENGINE_INTERNAL_HPP
#define PLANKTON_ENGINE_INTERNAL_HPP

#include "z3++.h"
#include "engine/encoding.hpp"

namespace plankton {
    
    struct InternalEncodingError : public std::exception {
        std::string msg;
        explicit InternalEncodingError(std::string msg) : msg("Internal encoding error: " + std::move(msg) + ".") {}
        [[nodiscard]] const char* what() const noexcept override { return msg.c_str(); }
    };
    
    inline z3::expr AsExpr(const InternalExpr& expr);
    inline z3::func_decl AsFuncDecl(const InternalExpr& expr);
    
    struct Z3Expr : public InternalExpr {
        z3::expr repr;
        
        explicit Z3Expr(const z3::expr& repr) : repr(repr) {}
    
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Negate() const override {
            return std::make_unique<Z3Expr>(!repr);
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> And(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr && AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Or(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr || AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Eq(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr == AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Neq(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr != AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Lt(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr < AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Leq(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr <= AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Gt(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr > AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Geq(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr >= AsExpr(other));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Implies(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(z3::implies(repr, AsExpr(other)));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Contains(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::expr' does not support 'operator()'");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Copy() const override {
            return std::make_unique<Z3Expr>(repr);
        }
    };
    
    struct Z3FuncDecl : public InternalExpr {
        z3::func_decl repr;
        
        explicit Z3FuncDecl(const z3::func_decl& repr) : repr(repr) {}
    
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Contains(const InternalExpr& other) const override {
            return std::make_unique<Z3Expr>(repr(AsExpr(other)));
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Eq(const InternalExpr& otherFuncDecl) const override {
            auto other = AsFuncDecl(otherFuncDecl);
            // TODO: is this too much of a hack?
            assert(repr.arity() == 1);
            assert(other.arity() == 1);
            assert(z3::eq(repr.domain(0), other.domain(0)));
            auto qv = repr.ctx().constant("__op-qv", repr.domain(0));
            auto expr = z3::forall(qv, repr(qv) == other(qv));
            return std::make_unique<Z3Expr>(expr);
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Neq(const InternalExpr& other) const override {
            return this->Eq(other)->Negate();
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Copy() const override {
            return std::make_unique<Z3FuncDecl>(repr);
        }
    
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Negate() const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator!'");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> And(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator&&'");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Or(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator||'");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Lt(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator<'");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Leq(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator<='");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Gt(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator>'");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Geq(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator>='");
        }
        [[nodiscard]] inline std::unique_ptr<InternalExpr> Implies(const InternalExpr&) const override {
            throw InternalEncodingError("'z3::func_decl' does not support 'operator>>'");
        }
    };
    
    inline z3::expr AsExpr(const InternalExpr& expr) {
        if (auto down = dynamic_cast<const Z3Expr*>(&expr)) return down->repr;
        throw InternalEncodingError("expected 'z3::expr'");
    }
    
    inline z3::expr AsExpr(const EExpr& expr) {
        return AsExpr(expr.Repr());
    }
    
    inline z3::func_decl AsFuncDecl(const InternalExpr& expr) {
        if (auto down = dynamic_cast<const Z3FuncDecl*>(&expr)) return down->repr;
        throw InternalEncodingError("expected 'z3::func_decl'");
    }
    
    inline z3::func_decl AsFuncDecl(const EExpr& expr) {
        return AsFuncDecl(expr.Repr());
    }
    
    struct Z3InternalStorage : public InternalStorage {
        z3::context context;
        z3::solver solver;
    
        explicit Z3InternalStorage() : context(), solver(context) {}
        
//        inline z3::expr_vector AsVector(const std::vector<EExpr>& vector) {
//            z3::expr_vector result(context);
//            for (const auto& elem : vector) result.push_back(AsExpr(elem.Repr()));
//            return result;
//        }
    };
    
    inline Z3InternalStorage& AsInternal(std::unique_ptr<InternalStorage>& object) {
        if (auto down = dynamic_cast<Z3InternalStorage*>(object.get())) return *down;
        throw InternalEncodingError("expected 'Z3Internals'");
    }
    
    inline z3::context& AsContext(std::unique_ptr<InternalStorage>& object) {
        return AsInternal(object).context;
    }
    
    inline z3::solver& AsSolver(std::unique_ptr<InternalStorage>& object) {
        return AsInternal(object).solver;
    }
    
    inline EExpr AsEExpr(const z3::expr& expr) {
        return EExpr(std::make_unique<Z3Expr>(expr));
    }
    
    inline EExpr AsEExpr(const z3::func_decl& expr) {
        return EExpr(std::make_unique<Z3FuncDecl>(expr));
    }
    
    inline EExpr AsEExpr(const InternalExpr& expr) {
        return EExpr(expr.Copy());
    }
    
    inline EExpr AsEExpr(const std::unique_ptr<InternalExpr>& expr) {
        return EExpr(expr->Copy());
    }
    
    inline EExpr AsEExpr(std::unique_ptr<InternalExpr>&& expr) {
        return EExpr(std::move(expr));
    }

} // namespace plankton

#endif //PLANKTON_ENGINE_INTERNAL_HPP
