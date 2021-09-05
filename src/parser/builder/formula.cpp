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
    inline std::unique_ptr<SymbolicVariable> FlowFromVar(T& context) {
        auto& var = builder.VariableByName(context.name->getText());
        if (var.type.sort != Sort::PTR)
            throw std::logic_error("Parse error: accessing flow through non-pointer variable."); // TODO: better error handling
            
        auto resource = plankton::TryGetResource(var, state);
        if (!resource)
            throw std::logic_error("internal error: cannot find flow."); // TODO: better error handling
    
        auto& node = resource->value->Decl();
        auto memory = plankton::TryGetResource(node, state);
        if (memory) return std::make_unique<SymbolicVariable>(memory->flow->Decl());
        throw std::logic_error("internal error: cannot find flow."); // TODO: better error handling
    }
    
    antlrcpp::Any visitAxiomCondition(PlanktonParser::AxiomConditionContext* ctx) override {
        auto expr = builder.MakeExpression(*ctx->binaryCondition());
        auto lhs = plankton::MakeSymbolic(*expr->lhs, state);
        auto rhs = plankton::MakeSymbolic(*expr->rhs, state);
        result = std::make_unique<StackAxiom>(expr->op, std::move(lhs), std::move(rhs));
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
};

inline std::unique_ptr<Axiom>
MakeAxiom(AstBuilder& builder, PlanktonParser::AxiomContext& context, const Formula& eval) {
    AxiomBuilder visitor(builder, eval);
    context.accept(&visitor);
    if (visitor.result) return std::move(visitor.result);
    throw std::logic_error("Internal error: could not create axiom."); // TODO: better error handling
}

std::unique_ptr<Formula> AstBuilder::MakeFormula(PlanktonParser::FormulaContext& context, const Formula& eval) {
    assert(!context.conjuncts.empty());
    if (context.conjuncts.size() == 1) return MakeAxiom(*this, *context.conjuncts.front(), eval);
    auto result = std::make_unique<SeparatingConjunction>();
    for (auto* axiomContext : context.conjuncts) result->Conjoin(MakeAxiom(*this, *axiomContext, eval));
    return result;
}

inline std::unique_ptr<SeparatingImplication>
MakeImplication(AstBuilder& builder, PlanktonParser::SeparatingImplicationContext& context, const Formula& eval) {
    auto result = std::make_unique<SeparatingImplication>();
    if (context.lhs) result->premise->Conjoin(builder.MakeFormula(*context.lhs, eval));
    result->conclusion->Conjoin(builder.MakeFormula(*context.rhs, eval));
    return result;
}

std::unique_ptr<Invariant> AstBuilder::MakeInvariant(PlanktonParser::InvariantContext& context, const Formula& eval) {
    auto result = std::make_unique<Invariant>();
    for (auto* impContext : context.separatingImplication()) {
        result->Conjoin(MakeImplication(*this, *impContext, eval));
    }
    return result;
}
