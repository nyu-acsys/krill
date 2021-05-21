#pragma once
#ifndef SOLVER_ENCODER
#define SOLVER_ENCODER

#include "z3++.h"
#include "cola/ast.hpp"
#include "heal/logic.hpp"

namespace solver {

    using ResourceList = std::set<const heal::SymbolicVariableDeclaration*>;

    struct ResourceCollector : public heal::DefaultLogicListener {
        ResourceList resources{};
        void enter(const heal::PointsToAxiom& obj) override { resources.insert(&obj.node->Decl()); }
        static ResourceList Collect(const heal::LogicObject& obj) {
            ResourceCollector collector;
            obj.accept(collector);
            return std::move(collector.resources);
        }
    };

    struct Z3Encoder : public heal::BaseLogicVisitor {
        z3::context& context;
        z3::solver& solver;
        Z3Encoder(z3::context& context, z3::solver& solver) : context(context), solver(solver), result(context) {}
        Z3Encoder(const Z3Encoder& other) = delete;

        z3::expr operator()(const heal::LogicObject& obj) { return Encode(obj); }
        z3::expr operator()(const cola::VariableDeclaration& decl) { return EncodeProgramVariable(decl); }
        z3::expr operator()(const heal::SymbolicVariableDeclaration& decl) { return EncodeSymbol(decl); }
        z3::func_decl operator()(const heal::SymbolicFlowDeclaration& decl) { return EncodeFlow(decl); }
        z3::expr QuantifiedVariable(cola::Sort sort) { return context.constant("__qv", EncodeSort(sort)); }

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
                     && MakeResourceGuarantee(ResourceCollector::Collect(obj));
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

        z3::expr MakeResourceGuarantee(const ResourceList& resourceList) {
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
        V GetOrCreate(std::map<K, V>& map, K key, F create) {
            auto find = map.find(key);
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

        z3::expr EncodeProgramVariable(const cola::VariableDeclaration& decl) {
            return GetOrCreate(vars, &decl, [this,&decl](){
                auto name = "__" + decl.name;
                auto expr = context.constant(name.c_str(), EncodeSort(decl.type.sort));
                return expr;
            });
        }

        z3::expr EncodeSymbol(const heal::SymbolicVariableDeclaration& decl) {
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

        z3::func_decl EncodeFlow(const heal::SymbolicFlowDeclaration& decl) {
            return GetOrCreate(flows, &decl, [this,&decl](){
                // create symbol
                auto name = "_V" + decl.name;
                auto expr = context.function(name.c_str(), EncodeSort(decl.type.sort), context.bool_sort());
                // add implicit bounds on data values
                auto qv = QuantifiedVariable(decl.type.sort);
                solver.add(z3::forall(qv, z3::implies(qv < MinData() && qv > MaxData(), !expr(qv))));
                return expr;
            });
        }

        z3::expr Encode(const heal::LogicObject& obj) {
            obj.accept(*this);
            return result;
        }

        template<typename T>
        z3::expr_vector EncodeAll(const T& elements) {
            z3::expr_vector vec(context);
            for (const auto& elem : elements) {
                vec.push_back(Encode(*elem));
            }
            return vec;
        }
    };


    static std::vector<bool> ComputeImplied(z3::context& context, z3::solver& solver, const z3::expr_vector& expressions) {
        // prepare required vectors
        z3::expr_vector variables(context);
        z3::expr_vector assumptions(context);
        z3::expr_vector consequences(context);

        // prepare solver
        for (unsigned int index = 0; index < expressions.size(); ++index) {
            std::string name = "__chk" + std::to_string(index);
            auto var = context.bool_const(name.c_str());
            variables.push_back(var);
            solver.add(var == expressions[(int) index]);
        }

        // check
        auto answer = solver.consequences(assumptions, variables, consequences);

        // create result
        std::vector<bool> result(expressions.size(), false);
        switch (answer) {
            case z3::unknown:
                throw std::logic_error("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");

            case z3::unsat:
                result.flip();
                return result;

            case z3::sat:
                for (unsigned int index = 0; index < expressions.size(); ++index) {
                    auto search = z3::implies(true, variables[(int) index]);
                    auto find = std::find_if(consequences.begin(), consequences.end(), [search](const auto& elem){
                        return z3::eq(search, elem);
                    });
                    if (find != consequences.end()) result[index] = true;
                }
                return result;
        }
    }

    //std::vector<bool> ComputeImplied(FootprintEncoding& encoding, const z3::expr_vector& expressions) {
    //    std::vector<bool> result;
    //
    //    for (const auto& expr : expressions) {
    //        encoding.solver.push();
    //        encoding.solver.add(!expr);
    //        auto res = encoding.solver.check();
    //        encoding.solver.pop();
    //        bool chk;
    //        switch (res) {
    //            case z3::unsat: chk = true; break;
    //            case z3::sat: chk = false; break;
    //            case z3::unknown: throw std::logic_error("Solving failed."); // TODO: better error handling
    //        }
    //        result.push_back(chk);
    //    }
    //
    //    return result;
    //}

    struct ImplicationCheckSet {
        z3::expr_vector expressions;
        std::deque<std::function<void(bool)>> callbacks;

        explicit ImplicationCheckSet(z3::context& context) : expressions(context), callbacks({}) {}
        void Add(const z3::expr& expression, std::function<void(bool)> callback) {
            expressions.push_back(expression);
            callbacks.push_back(std::move(callback));
        }
    };

    static void ComputeImpliedCallback(z3::context& context, z3::solver& solver, const ImplicationCheckSet& checks) {
        auto implied = ComputeImplied(context, solver, checks.expressions);
        assert(checks.expressions.size() == implied.size());
        for (std::size_t index = 0; index < implied.size(); ++index) {
            checks.callbacks.at(index)(implied.at(index));
        }
    }

}

#endif