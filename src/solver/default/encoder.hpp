#pragma once
#ifndef SOLVER_ENCODER
#define SOLVER_ENCODER

#include <set>
#include "z3++.h"
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/collect.hpp"

namespace solver {

    struct NoRenaming {
        static inline const cola::VariableDeclaration* Rename(const cola::VariableDeclaration* decl) { return decl; }
        static inline const heal::SymbolicVariableDeclaration* Rename(const heal::SymbolicVariableDeclaration* decl) { return decl; }
        static inline const heal::SymbolicFlowDeclaration* Rename(const heal::SymbolicFlowDeclaration* decl) { return decl; }
    };

    template<class Renaming=NoRenaming>
    struct Z3Encoder : public heal::BaseLogicVisitor {
        Renaming renaming;
        z3::context& context;
        z3::solver& solver;
        Z3Encoder(z3::context& context, z3::solver& solver) : context(context), solver(solver), result(context) {}
        Z3Encoder(const Z3Encoder& other) = delete;

        inline z3::expr operator()(const heal::LogicObject& obj) { return Encode(obj); }
        inline z3::expr operator()(const cola::VariableDeclaration& decl) { return EncodeProgramVariable(decl); }
        inline z3::expr operator()(const heal::SymbolicVariableDeclaration& decl) { return EncodeSymbol(decl); }
        inline z3::func_decl operator()(const heal::SymbolicFlowDeclaration& decl) { return EncodeFlow(decl); }
        inline z3::expr QuantifiedVariable(cola::Sort sort) { return context.constant("__qv", EncodeSort(sort)); }
        inline z3::expr MakeMemoryEquality(const heal::PointsToAxiom& memory, const heal::PointsToAxiom& other) {
            auto qv = QuantifiedVariable(memory.flow.get().type.sort);
            z3::expr_vector eq(context);
            eq.push_back(Encode(*memory.node) == Encode(*other.node));
            eq.push_back(z3::forall(qv, EncodeFlow(memory.flow.get())(qv) == EncodeFlow(other.flow.get())(qv)));
            assert(memory.node->Type() == other.node->Type());
            for (const auto& [field, value] : memory.fieldToValue) {
                eq.push_back(Encode(*value) == Encode(*other.fieldToValue.at(field)));
            }
            return z3::mk_and(eq);
        }
        inline z3::expr MakeNullCheck(const heal::SymbolicVariableDeclaration& decl) {
            return EncodeSymbol(decl) == Null();
        }

        void visit(const heal::SymbolicVariable& obj) override { result = EncodeSymbol(obj.Decl()); }
        void visit(const heal::SymbolicBool& obj) override { result = context.bool_val(obj.value); }
        void visit(const heal::SymbolicNull& /*obj*/) override { result = Null(); }
        void visit(const heal::SymbolicMin& /*obj*/) override { result = MinData(); }
        void visit(const heal::SymbolicMax& /*obj*/) override { result = MaxData(); }
        void visit(const heal::PointsToAxiom& /*obj*/) override { result = context.bool_val(true); }
//        void visit(const heal::EqualsToAxiom& /*obj*/) override { result = context.bool_val(true); }
        void visit(const heal::EqualsToAxiom& obj) override { result = EncodeProgramVariable(obj.variable->decl) == Encode(*obj.value); }
        void visit(const heal::ObligationAxiom& /*obj*/) override { result = context.bool_val(true); }
        void visit(const heal::FulfillmentAxiom& /*obj*/) override { result = context.bool_val(true); }
        void visit(const heal::Annotation& obj) override { result = Encode(*obj.now); }

        void visit(const heal::InflowContainsValueAxiom& obj) override {
            result = EncodeFlow(obj.flow)(Encode(*obj.value)) == context.bool_val(true);
        }
        void visit(const heal::InflowContainsRangeAxiom& obj) override {
            auto qv = QuantifiedVariable(obj.valueLow->Sort());
            auto premise = (Encode(*obj.valueLow) <= qv) && (qv <= Encode(*obj.valueHigh));
            auto conclusion = EncodeFlow(obj.flow)(qv);
            result = z3::forall(qv, z3::implies(premise, conclusion));
        }
        void visit(const heal::InflowEmptinessAxiom& obj) override {
            auto qv = QuantifiedVariable(obj.flow.get().type.sort);
            result = z3::exists(qv, EncodeFlow(obj.flow)(qv));
            if (obj.isEmpty) result = !result;
        }

        void visit(const heal::SeparatingConjunction& obj) override {
            result = z3::mk_and(EncodeAll(obj.conjuncts))
                     && MakeResourceGuarantee(heal::CollectMemoryNodes(obj));
        }
        void visit(const heal::StackDisjunction& obj) override {
            result = z3::mk_or(EncodeAll(obj.axioms));
        }
        void visit(const heal::SeparatingImplication& obj) override {
            result = z3::implies(Encode(*obj.premise), Encode(*obj.conclusion));
        }
        void visit(const heal::SymbolicAxiom& obj) override {
            auto lhs = Encode(*obj.lhs);
            auto rhs = Encode(*obj.rhs);
            switch (obj.op) {
                case heal::SymbolicAxiom::EQ: result = lhs == rhs; break;
                case heal::SymbolicAxiom::NEQ: result = lhs != rhs; break;
                case heal::SymbolicAxiom::LEQ: result = lhs <= rhs; break;
                case heal::SymbolicAxiom::LT: result = lhs < rhs; break;
                case heal::SymbolicAxiom::GEQ: result = lhs >= rhs; break;
                case heal::SymbolicAxiom::GT: result = lhs > rhs; break;
            }
        }

        z3::expr MakeResourceGuarantee(const std::set<const heal::SymbolicVariableDeclaration*>& resourceList) {
            if (resourceList.empty()) return context.bool_val(true);
            z3::expr_vector vec(context);
            vec.push_back(Null());
            for (const auto* resource : resourceList) {
                vec.push_back(EncodeSymbol(*resource));
            }
            return z3::distinct(vec);
        }

    private:
        static constexpr int NULL_VALUE = 0;
        static constexpr int MIN_VALUE = -65536;
        static constexpr int MAX_VALUE = 65536;

        z3::expr result;
        std::map<const cola::VariableDeclaration*, z3::expr> vars;
        std::map<const heal::SymbolicVariableDeclaration*, z3::expr> symbols;
        std::map<const heal::SymbolicFlowDeclaration*, z3::func_decl> flows;

        z3::expr MinData(){ return context.int_val(MIN_VALUE); };
        z3::expr MaxData(){ return context.int_val(MAX_VALUE); };
        z3::expr Null(){ return context.int_val(NULL_VALUE); };

        template<typename K, typename V, typename F>
        inline V GetOrCreate(std::map<K, V>& map, K key, F create) {
            auto find = map.find(renaming.Rename(key));
            if (find != map.end()) return find->second;
            auto newValue = create();
            map.emplace(key, newValue);
            return newValue;
        }

        inline z3::sort EncodeSort(cola::Sort sort) {
            switch (sort) {
                case cola::Sort::VOID: throw std::logic_error("Cannot encode 'VOID' sort."); // TODO: better error handling
                case cola::Sort::BOOL: return context.bool_sort();
                default: return context.int_sort();
            }
        }

        inline z3::expr EncodeProgramVariable(const cola::VariableDeclaration& decl) {
            return GetOrCreate(vars, &decl, [this,&decl](){
                auto name = "__" + decl.name;
                auto expr = context.constant(name.c_str(), EncodeSort(decl.type.sort));
                return expr;
            });
        }

        inline z3::expr EncodeSymbol(const heal::SymbolicVariableDeclaration& decl) {
            return GetOrCreate(symbols, &decl, [this,&decl](){
                // create symbol
                auto name = "_v" + decl.name;
                auto expr = context.constant(name.c_str(), EncodeSort(decl.type.sort));
                // add implicit bounds on data values
                if (decl.type.sort == cola::Sort::DATA) {
                    solver.add(MinData() <= expr && expr <= MaxData());
                }
                return expr;
            });
        }

        inline z3::func_decl EncodeFlow(const heal::SymbolicFlowDeclaration& decl) {
            return GetOrCreate(flows, &decl, [this,&decl](){
                // create symbol
                auto name = "_V" + decl.name;
                auto expr = context.function(name.c_str(), EncodeSort(decl.type.sort), context.bool_sort());
                // add implicit bounds on data values
                auto qv = QuantifiedVariable(decl.type.sort);
                solver.add(z3::forall(qv, z3::implies(qv < MinData() || qv > MaxData(), !expr(qv))));
                return expr;
            });
        }

        inline z3::expr Encode(const heal::LogicObject& obj) {
            obj.accept(*this);
            return result;
        }

        template<typename T>
        inline z3::expr_vector EncodeAll(const T& elements) {
            z3::expr_vector vec(context);
            for (const auto& elem : elements) {
                vec.push_back(Encode(*elem));
            }
            return vec;
        }
    };


    bool ComputeImplied(z3::solver& solver, const z3::expr& premise, const z3::expr& conclusion);

    std::vector<bool> ComputeImplied(z3::solver& solver, const z3::expr_vector& expressions);

    struct ImplicationCheckSet {
        z3::expr_vector expressions;
        std::deque<std::function<void(bool)>> callbacks;

        explicit ImplicationCheckSet(z3::context& context) : expressions(context), callbacks({}) {}
        void Add(const z3::expr& expression, std::function<void(bool)> callback) {
            expressions.push_back(expression);
            callbacks.push_back(std::move(callback));
        }
    };

    void ComputeImpliedCallback(z3::solver& solver, const ImplicationCheckSet& checks);

}

#endif