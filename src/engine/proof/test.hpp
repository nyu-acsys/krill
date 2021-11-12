#pragma once
#ifndef PLANKTON_SOLVER_TEST_HPP
#define PLANKTON_SOLVER_TEST_HPP

#include "logics/ast.hpp"

namespace plankton {

    inline std::deque<std::unique_ptr<HeapEffect>> MakeTestInterference(const Type& nodeType);
    inline std::pair<std::unique_ptr<Annotation>, std::unique_ptr<MemoryWrite>> MakeTestState(const Program& program);


    inline std::unique_ptr<HeapEffect> MakeTestEffect(SymbolFactory& factory, const Type& nodeType) {
        auto pre = plankton::MakeSharedMemory(factory.GetFreshFO(nodeType), Type::Data(), factory);
        auto post = plankton::Copy(*pre);
        auto context = std::make_unique<SeparatingConjunction>();
        return std::make_unique<HeapEffect>(std::move(pre), std::move(post), std::move(context));
    }

    inline std::unique_ptr<HeapEffect> MakeTestEffect1(SymbolFactory& factory, const Type& nodeType) {
        // [ x |=> G(inflow=A, marked=b, next=c, val=d) ~~> x |=> G(inflow=A, marked=b, next=c', val=d) | A != ∅ * b == false * c != nullptr * d < ∞ ]
        auto eff = MakeTestEffect(factory, nodeType);
        eff->post->fieldToValue.at("next") = std::make_unique<SymbolicVariable>(factory.GetFreshFO(nodeType));
        auto ctx = std::make_unique<SeparatingConjunction>();
        ctx->Conjoin(std::make_unique<InflowEmptinessAxiom>(eff->pre->flow->Decl(), false));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("marked")->Decl()), std::make_unique<SymbolicBool>(false)));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::NEQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("next")->Decl()), std::make_unique<SymbolicNull>()));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::LT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMax>()));
        eff->context = std::move(ctx);
        return eff;
    }

    inline std::unique_ptr<HeapEffect> MakeTestEffect2(SymbolFactory& factory, const Type& nodeType) {
        // [ x |=> G(inflow=A, marked=b, next=c, val=d) ~~> x |=> G(inflow=A, marked=b', next=c, val=d) | A != ∅ * b == false * c != nullptr * d > -∞ * d < ∞ ]
        auto eff = MakeTestEffect(factory, nodeType);
        eff->post->fieldToValue.at("marked") = std::make_unique<SymbolicVariable>(factory.GetFreshFO(Type::Bool()));
        auto ctx = std::make_unique<SeparatingConjunction>();
        ctx->Conjoin(std::make_unique<InflowEmptinessAxiom>(eff->pre->flow->Decl(), false));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("marked")->Decl()), std::make_unique<SymbolicBool>(false)));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::NEQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("next")->Decl()), std::make_unique<SymbolicNull>()));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::GT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMin>()));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::LT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMax>()));
        eff->context = std::move(ctx);
        return eff;
    }

    inline std::unique_ptr<HeapEffect> MakeTestEffect3(SymbolFactory& factory, const Type& nodeType) {
        // [ x |=> G(inflow=A, marked=b, next=c, val=d) ~~> x |=> G(inflow=A', marked=b, next=c, val=d) | A != ∅ * d > -∞ ]
        auto eff = MakeTestEffect(factory, nodeType);
        eff->post->flow = std::make_unique<SymbolicVariable>(factory.GetFreshSO(Type::Data()));
        auto ctx = std::make_unique<SeparatingConjunction>();
        ctx->Conjoin(std::make_unique<InflowEmptinessAxiom>(eff->pre->flow->Decl(), false));
        ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::GT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMin>()));
        eff->context = std::move(ctx);
        return eff;
    }

    inline std::deque<std::unique_ptr<HeapEffect>> MakeTestInterference(const Type& nodeType) {
        SymbolFactory factory;
        std::deque<std::unique_ptr<HeapEffect>> result;
        result.push_back(MakeTestEffect1(factory, nodeType));
        result.push_back(MakeTestEffect2(factory, nodeType));
        result.push_back(MakeTestEffect3(factory, nodeType));
        return result;
    }

    struct TestGen {
        SymbolFactory factory;
        std::unique_ptr<Annotation> annotation;
        std::unique_ptr<MemoryWrite> command;

        static inline std::unique_ptr<SymbolicExpression> MkBool(bool value) {
            return std::make_unique<SymbolicBool>(value);
        }
        static inline std::unique_ptr<SymbolicExpression> MkNull() {
            return std::make_unique<SymbolicNull>();
        }
        static inline std::unique_ptr<SymbolicExpression> MkMin() {
            return std::make_unique<SymbolicMin>();
        }
        static inline std::unique_ptr<SymbolicExpression> MkMax() {
            return std::make_unique<SymbolicMax>();
        }
        inline void AddMem(const SymbolDeclaration& adr, const SymbolDeclaration& flow, const SymbolDeclaration& marked, const SymbolDeclaration& next, const SymbolDeclaration& val) {
            auto node = std::make_unique<SymbolicVariable>(adr);
            auto inflow = std::make_unique<SymbolicVariable>(flow);
            std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
            fieldToValue["marked"] = std::make_unique<SymbolicVariable>(marked);
            fieldToValue["next"] = std::make_unique<SymbolicVariable>(next);
            fieldToValue["val"] = std::make_unique<SymbolicVariable>(val);
            annotation->Conjoin(std::make_unique<SharedMemoryCore>(std::move(node), std::move(inflow), std::move(fieldToValue)));
        }
        inline void AddVar(const VariableDeclaration& var, const SymbolDeclaration& value) {
            annotation->Conjoin(std::make_unique<EqualsToAxiom>(var, value));
        }
        inline void AddOp(BinaryOperator op, const SymbolDeclaration& decl, std::unique_ptr<SymbolicExpression> expr) {
            annotation->Conjoin(std::make_unique<StackAxiom>(op, std::make_unique<SymbolicVariable>(decl), std::move(expr)));
        }
        inline void AddOp(BinaryOperator op, const SymbolDeclaration& decl, const SymbolDeclaration& other) {
            AddOp(op, decl, std::make_unique<SymbolicVariable>(other));
        }
        inline void AddEq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
            AddOp(BinaryOperator::EQ, decl, other);
        }
        inline void AddEq(const SymbolDeclaration& decl, std::unique_ptr<SymbolicExpression> expr) {
            AddOp(BinaryOperator::EQ, decl, std::move(expr));
        }
        inline void AddNeq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
            AddOp(BinaryOperator::NEQ, decl, other);
        }
        inline void AddNeq(const SymbolDeclaration& decl, std::unique_ptr<SymbolicExpression> expr) {
            AddOp(BinaryOperator::NEQ, decl, std::move(expr));
        }
        inline void AddLt(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
            AddOp(BinaryOperator::LT, decl, other);
        }
        inline void AddLt(const SymbolDeclaration& decl, std::unique_ptr<SymbolicExpression> expr) {
            AddOp(BinaryOperator::LT, decl, std::move(expr));
        }
        inline void AddLte(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
            AddOp(BinaryOperator::LEQ, decl, other);
        }
        inline void AddLte(const SymbolDeclaration& decl, std::unique_ptr<SymbolicExpression> expr) {
            AddOp(BinaryOperator::LEQ, decl, std::move(expr));
        }
        inline void AddGt(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
            AddOp(BinaryOperator::GT, decl, other);
        }
        inline void AddGt(const SymbolDeclaration& decl, std::unique_ptr<SymbolicExpression> expr) {
            AddOp(BinaryOperator::GT, decl, std::move(expr));
        }
        inline void AddGte(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
            AddOp(BinaryOperator::GEQ, decl, other);
        }
        inline void AddGte(const SymbolDeclaration& decl, std::unique_ptr<SymbolicExpression> expr) {
            AddOp(BinaryOperator::GEQ, decl, std::move(expr));
        }
        inline void AddNonEmpty(const SymbolDeclaration& flow) {
            annotation->Conjoin(std::make_unique<InflowEmptinessAxiom>(flow, false));
        }
        inline void AddContains(const SymbolDeclaration& value, const SymbolDeclaration& flow) {
            annotation->Conjoin(std::make_unique<InflowContainsValueAxiom>(flow, value));
        }

        static inline const VariableDeclaration& FindVariable(const Program& program, const std::string& name) {
            struct : public ProgramListener {
                std::deque<const VariableDeclaration*> decls;
                void Enter(const VariableDeclaration& object) override { decls.push_back(&object); }
            } collector;
            program.Accept(collector);

            for (const auto* var : collector.decls) {
                if (var->name == name) return *var;
            }
            throw std::logic_error("---broken debug---");
        }

        explicit TestGen(const Program& program) : annotation(std::make_unique<Annotation>()) {
            auto& Head = FindVariable(program, "Head");
            auto& Tail = FindVariable(program, "Tail");
            auto& left_1 = FindVariable(program, "left_1");
            auto& right_1 = FindVariable(program, "right_1");
            auto& key_1 = FindVariable(program, "key_1");
            auto& k_1 = FindVariable(program, "k_1");
            auto& rnext_1 = FindVariable(program, "rnext_1");
            auto& rmark_1 = FindVariable(program, "rmark_1");
            auto& lnext_1 = FindVariable(program, "lnext_1");
            auto& key = FindVariable(program, "key");
            auto& left = FindVariable(program, "left");
            auto& right = FindVariable(program, "right");
            auto& k = FindVariable(program, "k");

            command = std::make_unique<MemoryWrite>(
                    std::make_unique<Dereference>(std::make_unique<VariableExpression>(left_1), "next"),
                    std::make_unique<VariableExpression>(right_1)
            );

            auto& flowType = Type::Data();
            auto& ptrType = *program.types.at(0);
            auto& dataType = Type::Data();
            auto& boolType = Type::Bool();

            auto& D103 = factory.GetFreshSO(flowType);
            auto& D107 = factory.GetFreshSO(flowType);
            auto& D111 = factory.GetFreshSO(flowType);
            auto& D123 = factory.GetFreshSO(flowType);
            auto& D143 = factory.GetFreshSO(flowType);
            auto& D20 = factory.GetFreshSO(flowType);
            auto& D48 = factory.GetFreshSO(flowType);
            auto& D52 = factory.GetFreshSO(flowType);
            auto& D56 = factory.GetFreshSO(flowType);
            auto& D66 = factory.GetFreshSO(flowType);
            auto& D67 = factory.GetFreshSO(flowType);
            auto& D69 = factory.GetFreshSO(flowType);
            auto& D70 = factory.GetFreshSO(flowType);
            auto& a113 = factory.GetFreshFO(ptrType);
            auto& a12 = factory.GetFreshFO(ptrType);
            auto& a17 = factory.GetFreshFO(ptrType);
            auto& a19 = factory.GetFreshFO(ptrType);
            auto& a22 = factory.GetFreshFO(ptrType);
            auto& a26 = factory.GetFreshFO(ptrType);
            auto& a27 = factory.GetFreshFO(ptrType);
            auto& a31 = factory.GetFreshFO(ptrType);
            auto& a39 = factory.GetFreshFO(ptrType);
            auto& a57 = factory.GetFreshFO(ptrType);
            auto& a58 = factory.GetFreshFO(ptrType);
            auto& a59 = factory.GetFreshFO(ptrType);
            auto& a6 = factory.GetFreshFO(ptrType);
            auto& a61 = factory.GetFreshFO(ptrType);
            auto& a62 = factory.GetFreshFO(ptrType);
            auto& a63 = factory.GetFreshFO(ptrType);
            auto& a64 = factory.GetFreshFO(ptrType);
            auto& a65 = factory.GetFreshFO(ptrType);
            auto& a68 = factory.GetFreshFO(ptrType);
            auto& a72 = factory.GetFreshFO(ptrType);
            auto& a76 = factory.GetFreshFO(ptrType);
            auto& a80 = factory.GetFreshFO(ptrType);
            auto& a81 = factory.GetFreshFO(ptrType);
            auto& a82 = factory.GetFreshFO(ptrType);
            auto& a83 = factory.GetFreshFO(ptrType);
            auto& a84 = factory.GetFreshFO(ptrType);
            auto& a9 = factory.GetFreshFO(ptrType);
            auto& a92 = factory.GetFreshFO(ptrType);
            auto& b100 = factory.GetFreshFO(boolType);
            auto& b16 = factory.GetFreshFO(boolType);
            auto& b2 = factory.GetFreshFO(boolType);
            auto& b34 = factory.GetFreshFO(boolType);
            auto& b8 = factory.GetFreshFO(boolType);
            auto& d11 = factory.GetFreshFO(dataType);
            auto& d18 = factory.GetFreshFO(dataType);
            auto& d23 = factory.GetFreshFO(dataType);
            auto& d30 = factory.GetFreshFO(dataType);
            auto& d32 = factory.GetFreshFO(dataType);
            auto& d37 = factory.GetFreshFO(dataType);
            auto& d51 = factory.GetFreshFO(dataType);
            auto& d55 = factory.GetFreshFO(dataType);

            AddEq(b2, MkBool(false));
            AddMem(a26, D123, b100, a92, d55);
            AddMem(a9, D111, b16, a12, d18);
            AddMem(a19, D143, b34, a39, d32);
            AddMem(a22, D20, b2, a6, d37);
            AddMem(a6, D107, b8, a9, d51);
            AddMem(a17, D70, b34, a113, d30);
            AddVar(Head, a17);
            AddVar(Tail, a19);
            AddVar(left_1, a22);
            AddVar(right_1, a9);
            AddVar(key_1, d11);
            AddVar(k_1, d18);
            AddVar(rnext_1, a12);
            AddVar(rmark_1, b16);
            AddVar(lnext_1, a26);
            AddVar(key, d11);
            AddVar(left, a27);
            AddVar(right, a31);
            AddVar(k, d23);
            AddNeq(a57, a39);
            AddNeq(d11, d37);
            AddNeq(d11, d30);
            AddGte(d11, d11);
            AddNeq(d11, d32);
            AddGte(d30, d30);
            AddNeq(a17, a57);
            AddNeq(a17, a39);
            AddNeq(a19, a58);
            AddNeq(a58, a39);
            AddNeq(a17, a19);
            AddNeq(a19, a39);
            AddNeq(d32, d37);
            AddNeq(d32, d30);
            AddGte(d32, d32);
            AddGte(d37, d37);
            AddNeq(a22, a59);
            AddNeq(a22, a39);
            AddGte(d51, d51);
            AddNeq(d51, d32);
            AddNeq(a61, a39);
            AddGte(d55, d55);
            AddNeq(d55, d32);
            AddNeq(a19, a6);
            AddNeq(a9, a6);
            AddNeq(a62, a6);
            AddNeq(a19, a26);
            AddNeq(a22, a26);
            AddNeq(a63, a26);
            AddNeq(a59, MkNull());
            AddNeq(a62, MkNull());
            AddNeq(a63, MkNull());
            AddNeq(a57, MkNull());
            AddNonEmpty(D48);
            AddNeq(a57, a17);
            AddContains(d32, D48);
            AddNeq(a59, a22);
            AddNeq(a9, a64);
            AddNeq(a59, a39);
            AddNeq(a63, a39);
            AddNeq(a62, a39);
            AddNeq(a6, a62);
            AddNeq(a26, a63);
            AddContains(d11, D52);
            AddNeq(a65, MkNull());
            AddNeq(a68, MkNull());
            AddEq(b8, MkBool(true));
            AddNeq(b8, MkBool(false));
            AddNeq(a72, MkNull());
            AddNeq(a76, MkNull());
            AddNeq(a80, MkNull());
            AddNonEmpty(D56);
            AddContains(d32, D56);
            AddNeq(a65, a22);
            AddNeq(a9, a76);
            AddNeq(a80, a17);
            AddNeq(b8, b34);
            AddNeq(a39, a80);
            AddNeq(a65, a39);
            AddNeq(a68, a39);
            AddNeq(a9, a39);
            AddNeq(a39, a76);
            AddNeq(a72, a39);
            AddNeq(a6, a68);
            AddNeq(a6, a39);
            AddNeq(a26, a72);
            AddNeq(a26, a39);
            AddContains(d11, D66);
            AddEq(a39, MkNull());
            AddNeq(a81, MkNull());
            AddNeq(d37, MkMax());
            AddLte(d37, MkMax());
            AddLt(d37, MkMax());
            AddGte(d37, MkMin());
            AddNeq(a6, MkNull());
            AddNeq(a9, MkNull());
            AddNeq(a12, MkNull());
            AddNeq(a26, MkNull());
            AddEq(d30, MkMin());
            AddNeq(d30, MkMax());
            AddLte(d30, MkMin());
            AddLte(d30, MkMax());
            AddLt(d30, MkMax());
            AddGte(d30, MkMin());
            AddEq(b34, MkBool(false));
            AddNeq(b34, MkBool(true));
            AddEq(d32, MkMax());
            AddNeq(d32, MkMin());
            AddLte(d32, MkMax());
            AddGte(d32, MkMin());
            AddGte(d32, MkMax());
            AddGt(d32, MkMin());
            AddNeq(a82, MkNull());
            AddNeq(d51, MkMax());
            AddLte(d51, MkMax());
            AddLt(d51, MkMax());
            AddGte(d51, MkMin());
            AddNeq(a83, MkNull());
            AddNeq(a84, MkNull());
            AddNeq(d55, MkMax());
            AddLte(d55, MkMax());
            AddLt(d55, MkMax());
            AddGte(d55, MkMin());
            AddNeq(a19, MkNull());
            AddNeq(d18, MkMax());
            AddLte(d18, MkMax());
            AddLt(d18, MkMax());
            AddGte(d18, MkMin());
            AddNeq(a17, MkNull());
            AddLte(d23, MkMax());
            AddGte(d23, MkMin());
            AddNeq(a22, MkNull());
            AddNeq(d11, MkMin());
            AddNeq(d11, MkMax());
            AddLte(d11, MkMax());
            AddLt(d11, MkMax());
            AddGte(d11, MkMin());
            AddGt(d11, MkMin());
            AddNonEmpty(D70);
            AddNonEmpty(D67);
            AddNeq(a39, a58);
            AddNeq(a58, a19);
            AddContains(d30, D70);
            AddNeq(a39, a19);
            AddContains(d32, D67);
            AddContains(d11, D69);
            AddNeq(a81, a22);
            AddNeq(b34, b8);
            AddNeq(a39, a57);
            AddNeq(a83, a17);
            AddNeq(a39, a63);
            AddLte(d30, d23);
            AddNeq(d30, d11);
            AddLte(d30, d11);
            AddLt(d30, d11);
            AddContains(d23, D70);
            AddContains(d11, D70);
            AddNeq(a39, a83);
            AddNeq(a39, a17);
            AddNeq(d30, d32);
            AddLte(d30, d32);
            AddLt(d30, d32);
            AddGte(d32, d23);
            AddNeq(d32, d11);
            AddGte(d32, d11);
            AddGt(d32, d11);
            AddNeq(a19, a17);
            AddContains(d32, D70);
            AddNeq(a39, a81);
            AddNeq(a39, a22);
            AddGte(d37, d30);
            AddNeq(d37, d32);
            AddLte(d37, d32);
            AddLt(d37, d32);
            AddNeq(d37, d11);
            AddLte(d37, d11);
            AddLt(d37, d11);
            AddNeq(a19, a22);
            AddContains(d37, D70);
            AddNeq(a39, a84);
            AddLte(d30, d51);
            AddNeq(d32, d51);
            AddGte(d32, d51);
            AddGt(d32, d51);
            AddNeq(d51, d11);
            AddLte(d51, d11);
            AddLt(d51, d11);
            AddContains(d51, D70);
            AddNeq(a39, a9);
            AddNeq(a39, a12);
            AddLte(d30, d18);
            AddNeq(d32, d18);
            AddGte(d32, d18);
            AddGt(d32, d18);
            AddContains(d18, D70);
            AddNeq(a39, a82);
            AddLte(d30, d55);
            AddNeq(d32, d55);
            AddGte(d32, d55);
            AddGt(d32, d55);
            AddContains(d55, D70);
            AddNeq(a39, a6);
            AddNeq(a6, a84);
            AddNeq(a6, a19);
            AddNeq(a39, a26);
            AddNeq(a26, a82);
            AddEq(b16, MkBool(true));
            AddContains(d11, D103);
            AddNeq(a26, a19);
            AddNeq(a26, a22);
            AddNeq(a9, a12);
            AddNeq(a6, a9);
            AddNeq(a9, a19);
        }
    };

    inline std::pair<std::unique_ptr<Annotation>, std::unique_ptr<MemoryWrite>> MakeTestState(const Program& program) {
        TestGen gen(program);
        return { std::move(gen.annotation), std::move(gen.command) };
    }
}

#endif //PLANKTON_SOLVER_TEST_HPP
