#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"
#include "logics//util.hpp"
#include "engine/util.hpp"

using namespace plankton;


struct AxiomBuilder : public PlanktonBaseVisitor {
    AstBuilder& builder;
    const Formula& state;
    std::unique_ptr<Axiom> result = nullptr;
    
    explicit AxiomBuilder(AstBuilder& builder, const Formula& state) : builder(builder), state(state) {}

    template<typename T>
    inline const MemoryAxiom* MemoryFromVar(T& context) {
        auto& var = builder.VariableByName(context.name->getText());
        if (var.type.sort != Sort::PTR)
            throw std::logic_error("Parse error: accessing flow through non-pointer variable."); // TODO: better error handling

        auto resource = plankton::TryGetResource(var, state);
        if (!resource)
            throw std::logic_error("Internal error: cannot find flow."); // TODO: better error handling

        auto& node = resource->value->Decl();
        return plankton::TryGetResource(node, state);
    }

    template<typename T>
    inline std::unique_ptr<SymbolicVariable> FlowFromVar(T& context) {
        auto memory = MemoryFromVar(context);
        if (memory) return std::make_unique<SymbolicVariable>(memory->flow->Decl());
        throw std::logic_error("internal error: cannot find flow."); // TODO: better error handling
    }

    template<typename T>
    inline std::unique_ptr<SymbolicVariable> FieldFromVar(T& context) {
        auto field = context.field->getText();
        auto memory = MemoryFromVar(context);
        if (memory) return std::make_unique<SymbolicVariable>(memory->fieldToValue.at(field)->Decl());
        throw std::logic_error("internal error: cannot find flow."); // TODO: better error handling
    }
    
    antlrcpp::Any visitAxiomCondition(PlanktonParser::AxiomConditionContext* ctx) override {
        auto expr = builder.MakeExpression(*ctx->binaryCondition());
        
        // convert comparisons with shared variables into an 'EqualsTo', rather than a 'StackAxiom'
        // TODO: this is quite ugly a hack
        auto handleShared = [this,&expr](auto& lhs, auto& rhs) {
            if (auto var = dynamic_cast<const VariableExpression*>(lhs.get())) {
                if (!var->Decl().isShared) return false;
                if (plankton::TryGetResource(var->Decl(), state)) return false;
                if (auto other = dynamic_cast<const VariableExpression*>(rhs.get())) {
                    if (expr->op == BinaryOperator::EQ && !other->Decl().isShared) {
                        auto& converted = plankton::Evaluate(*other, state);
                        result = std::make_unique<EqualsToAxiom>(var->Decl(), converted);
                        return true;
                    }
                }
                throw std::logic_error("Parse error: '" + plankton::ToString(*expr) + "' not allowed, '" + var->Decl().name + "' may only be compared by equality to a non-shared variable here."); // TODO: better error handling
            }
            return false;
        };
        
        if (!handleShared(expr->lhs, expr->rhs) && !handleShared(expr->rhs, expr->lhs)) {
            auto lhs = plankton::MakeSymbolic(*expr->lhs, state);
            auto rhs = plankton::MakeSymbolic(*expr->rhs, state);
            result = std::make_unique<StackAxiom>(expr->op, std::move(lhs), std::move(rhs));
        }
        return nullptr;
    }
    
    antlrcpp::Any visitAxiomFlowEmpty(PlanktonParser::AxiomFlowEmptyContext* ctx) override {
        auto flow = FlowFromVar(*ctx);
        result = std::make_unique<InflowEmptinessAxiom>(std::move(flow), true);
        return false;
    }
    
    antlrcpp::Any visitAxiomFlowNonEmpty(PlanktonParser::AxiomFlowNonEmptyContext* ctx) override {
        auto flow = FlowFromVar(*ctx);
        result = std::make_unique<InflowEmptinessAxiom>(std::move(flow), false);
        return false;
    }
    
    antlrcpp::Any visitAxiomFlowValue(PlanktonParser::AxiomFlowValueContext* ctx) override {
        auto flow = FlowFromVar(*ctx);
        VariableExpression valueExpr(builder.VariableByName(ctx->member->getText()));
        auto value = std::make_unique<SymbolicVariable>(plankton::Evaluate(valueExpr, state));
        result = std::make_unique<InflowContainsValueAxiom>(std::move(flow), std::move(value));
        return nullptr;
    }

    antlrcpp::Any visitAxiomFlowRange(PlanktonParser::AxiomFlowRangeContext* ctx) override {
        auto flow = FlowFromVar(*ctx);
        auto low = plankton::MakeSymbolic(*builder.MakeExpression(*ctx->low), state);
        auto high = plankton::MakeSymbolic(*builder.MakeExpression(*ctx->high), state);
        result = std::make_unique<InflowContainsRangeAxiom>(std::move(flow), std::move(low), std::move(high));
        return nullptr;
    }

    antlrcpp::Any visitAxiomUnlocked(PlanktonParser::AxiomUnlockedContext* ctx) override {
        auto field = FieldFromVar(*ctx);
        result = std::make_unique<StackAxiom>(BinaryOperator::EQ, std::move(field), std::make_unique<SymbolicUnlocked>());
        return nullptr;
    }

    antlrcpp::Any visitAxiomLocked(PlanktonParser::AxiomLockedContext* ctx) override {
        auto field = FieldFromVar(*ctx);
        result = std::make_unique<StackAxiom>(BinaryOperator::NEQ, std::move(field), std::make_unique<SymbolicUnlocked>());
        return nullptr;
    }
};

inline std::unique_ptr<Axiom>
MakeAxiom(AstBuilder& builder, PlanktonParser::AxiomContext& context, const Formula& eval) {
    AxiomBuilder visitor(builder, eval);
    context.accept(&visitor);
    if (visitor.result) return std::move(visitor.result);
    throw std::logic_error("Internal error: could not create axiom."); // TODO: better error handling
}

inline std::unique_ptr<Formula>
MakeFormula(AstBuilder& builder, PlanktonParser::FormulaContext& context, const Formula& eval) {
    assert(!context.conjuncts.empty());
    if (context.conjuncts.size() == 1) return MakeAxiom(builder, *context.conjuncts.front(), eval);
    auto result = std::make_unique<SeparatingConjunction>();
    for (auto* axiomContext : context.conjuncts) result->Conjoin(MakeAxiom(builder, *axiomContext, eval));
    return result;
}

struct VariableFinder : public PlanktonBaseVisitor {
    std::set<std::string> names;
    template<typename T>
    antlrcpp::Any Handle(T& context) {
        names.insert(context.name->getText());
        return nullptr;
    }
    antlrcpp::Any visitDeref(PlanktonParser::DerefContext* ctx) override { return Handle(*ctx); }
    antlrcpp::Any visitExprSimpleIdentifier(PlanktonParser::ExprSimpleIdentifierContext* ctx) override { return Handle(*ctx); }
    antlrcpp::Any visitAxiomFlowEmpty(PlanktonParser::AxiomFlowEmptyContext* ctx) override { return Handle(*ctx); }
    antlrcpp::Any visitAxiomFlowNonEmpty(PlanktonParser::AxiomFlowNonEmptyContext* ctx) override { return Handle(*ctx); }
    antlrcpp::Any visitAxiomFlowValue(PlanktonParser::AxiomFlowValueContext* ctx) override { return Handle(*ctx); }
    antlrcpp::Any visitAxiomFlowRange(PlanktonParser::AxiomFlowRangeContext* ctx) override { return Handle(*ctx); }
    antlrcpp::Any visitAxiomUnlocked(PlanktonParser::AxiomUnlockedContext* ctx) override { return Handle(*ctx); }
    antlrcpp::Any visitAxiomLocked(PlanktonParser::AxiomLockedContext* ctx) override { return Handle(*ctx); }
};

inline const SymbolDeclaration* GetEvalValue(const Formula& eval) {
    auto res = plankton::Collect<EqualsToAxiom>(eval, [&eval](const auto& obj){
        return !obj.Variable().isShared && plankton::TryGetResource(obj.Variable(), eval);
    });
    if (res.size() == 1) return &(**res.begin()).Value();
    return nullptr;
}

inline std::unique_ptr<NonSeparatingImplication>
MakeImplication(AstBuilder& builder, PlanktonParser::ImplicationContext& context, const Formula& eval) {
    auto result = std::make_unique<NonSeparatingImplication>();
    if (context.lhs) {
        result->premise->Conjoin(MakeFormula(builder, *context.lhs, eval));
        result->conclusion->Conjoin(MakeFormula(builder, *context.rhs, eval));
    } else {
        auto value = GetEvalValue(eval);
        if (value) {
            VariableFinder finder;
            context.rhs->accept(&finder);
            for (const auto& name : finder.names) {
                auto& var = builder.VariableByName(name);
                if (!var.isShared) continue;
                if (plankton::TryGetResource(var, eval)) continue;
                result->premise->Conjoin(std::make_unique<EqualsToAxiom>(var, *value));
            }
        }
        auto extendedEval = plankton::Copy(*result->premise);
        extendedEval->Conjoin(plankton::Copy(eval));
        plankton::Simplify(*extendedEval);
        result->conclusion->Conjoin(MakeFormula(builder, *context.rhs, *extendedEval));
    }
    return result;
}

std::unique_ptr<ImplicationSet> AstBuilder::MakeInvariant(PlanktonParser::InvariantContext& context, const Formula& eval) {
    auto result = std::make_unique<ImplicationSet>();
    for (auto* impContext : context.implication()) {
        result->Conjoin(MakeImplication(*this, *impContext, eval));
    }
    return result;
}
