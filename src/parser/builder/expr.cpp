#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"
#include "programs/util.hpp"

using namespace plankton;


inline std::unique_ptr<ValueExpression> MakeBool(bool value) {
    if (value) return std::make_unique<TrueValue>();
    else return std::make_unique<FalseValue>();
}

std::unique_ptr<ValueExpression> AstBuilder::MakeExpression(PlanktonParser::ExpressionContext& context) {
    if (context.simpleExpression()) return MakeExpression(*context.simpleExpression());
    if (context.deref()) return MakeExpression(*context.deref());
    throw std::logic_error("Internal error: unrecognized expression");
}

std::unique_ptr<Dereference> AstBuilder::MakeExpression(PlanktonParser::DerefContext& context) {
    auto var = std::make_unique<VariableExpression>(VariableByName(context.name->getText()));
    auto fieldName = context.field->getText();
    if (var->GetType().GetField(fieldName).has_value()) {
        return std::make_unique<Dereference>(std::move(var), fieldName);
    }
    throw std::logic_error("Parser error: variable of type '" + var->GetType().name + "' has no member named '" + fieldName + "'."); // TODO: better error handling
}

struct ValueBuilder : public PlanktonBaseVisitor {
    std::unique_ptr<SimpleExpression> result = nullptr;

    template<typename T>
    inline antlrcpp::Any Handle() { result = std::make_unique<T>(); return nullptr; }
    antlrcpp::Any visitValueTrue(PlanktonParser::ValueTrueContext*) override { return Handle<TrueValue>(); }
    antlrcpp::Any visitValueFalse(PlanktonParser::ValueFalseContext*) override { return Handle<FalseValue>(); }
    antlrcpp::Any visitValueMax(PlanktonParser::ValueMaxContext*) override { return Handle<MaxValue>(); }
    antlrcpp::Any visitValueMin(PlanktonParser::ValueMinContext*) override { return Handle<MinValue>(); }
    antlrcpp::Any visitValueNull(PlanktonParser::ValueNullContext*) override { return Handle<NullValue>(); }
};

std::unique_ptr<SimpleExpression> AstBuilder::MakeExpression(PlanktonParser::ValueContext& context) {
    ValueBuilder builder;
    context.accept(&builder);
    if (builder.result) return std::move(builder.result);
    throw std::logic_error("Internal error: could not create value."); // TODO: better error handling
}

struct SimpleExpressionBuilder : public PlanktonBaseVisitor {
    AstBuilder& builder;
    std::unique_ptr<SimpleExpression> result = nullptr;
    
    explicit SimpleExpressionBuilder(AstBuilder& builder) : builder(builder) {}
    
    antlrcpp::Any visitExprSimpleIdentifier(PlanktonParser::ExprSimpleIdentifierContext* ctx) override {
        result = std::make_unique<VariableExpression>(builder.VariableByName(ctx->name->getText()));
        return nullptr;
    }

    antlrcpp::Any visitExprSimpleValue(PlanktonParser::ExprSimpleValueContext* ctx) override {
        result = builder.MakeExpression(*ctx->value());
        return nullptr;
    }
};

std::unique_ptr<SimpleExpression> AstBuilder::MakeExpression(PlanktonParser::SimpleExpressionContext& context) {
    SimpleExpressionBuilder builder(*this);
    context.accept(&builder);
    if (builder.result) return std::move(builder.result);
    throw std::logic_error("Internal error: could not create simple expression."); // TODO: better error handling
}

struct NegatedExpressionBuilder : public PlanktonBaseVisitor {
    AstBuilder& builder;
    std::unique_ptr<ValueExpression> expr = nullptr;
    bool negated = false;
    
    explicit NegatedExpressionBuilder(AstBuilder& builder) : builder(builder) {}
    
    template<typename T>
    inline antlrcpp::Any Handle(T& ctx, bool neg) {
        expr = builder.MakeExpression(*ctx.expression());
        if (neg && expr->GetType() != Type::Bool()) {
            throw std::logic_error("Parse error: operator '!' not allowed with value of type '" + expr->GetType().name + "'."); // TODO: better error handling
        }
        negated = neg;
        return nullptr;
    }
    
    antlrcpp::Any visitCondSimpleExpr(PlanktonParser::CondSimpleExprContext* ctx) override { return Handle(*ctx, false); }
    antlrcpp::Any visitCondSimpleNegation(PlanktonParser::CondSimpleNegationContext* ctx) override { return Handle(*ctx, true); }
};

std::pair<std::unique_ptr<ValueExpression>, bool> AstBuilder::MakeExpression(PlanktonParser::SimpleConditionContext& context) {
    NegatedExpressionBuilder builder(*this);
    context.accept(&builder);
    if (builder.expr) return { std::move(builder.expr), builder.negated };
    throw std::logic_error("Internal error: could not create expression.");
}

inline bool IsOperatorCompatible(BinaryOperator op, const Type& lhs, const Type& rhs) {
    if (lhs.sort != rhs.sort) return false;
    if (lhs.sort == Sort::VOID) return false;
    
    switch (op) {
        case BinaryOperator::EQ:
        case BinaryOperator::NEQ:
            return true;

        case BinaryOperator::LEQ:
        case BinaryOperator::LT:
        case BinaryOperator::GEQ:
        case BinaryOperator::GT:
            return lhs.sort == Sort::DATA;
    }
    throw;
}

struct BinaryConditionBuilder : public PlanktonBaseVisitor {
    AstBuilder& builder;
    std::unique_ptr<BinaryExpression> result = nullptr;
    
    explicit BinaryConditionBuilder(AstBuilder& builder) : builder(builder) {}
    
    antlrcpp::Any visitCondBinarySimple(PlanktonParser::CondBinarySimpleContext* ctx) override {
        auto [expr, negated] = builder.MakeExpression(*ctx->simpleCondition());
        if (expr->GetType() == Type::Bool()) {
            result = std::make_unique<BinaryExpression>(BinaryOperator::EQ, std::move(expr), MakeBool(!negated));
            return nullptr;
        }
        
        throw std::logic_error("Parse error: cannot implicitly convert expression of type '" + expr->GetType().name + "' to bool.");
    }
    
    template<BinaryOperator Op, typename T>
    inline antlrcpp::Any Handle(T& ctx) {
        auto[lhs, lhsNegated] = builder.MakeExpression(*ctx.lhs);
        auto[rhs, rhsNegated] = builder.MakeExpression(*ctx.rhs);
        
        if (!lhs->GetType().Comparable(rhs->GetType()))
            throw std::logic_error("Parse error: incomparable types '" + lhs->GetType().name + "' and '" + rhs->GetType().name + "'."); // TODO: better error handling
        if (!IsOperatorCompatible(Op, lhs->GetType(), rhs->GetType()))
            throw std::logic_error("Parse error: operator " + plankton::ToString(Op) + " not allowed here."); // TODO: better error handling
        
        result = std::make_unique<BinaryExpression>(Op, std::move(lhs), std::move(rhs));
        if (lhsNegated ^ rhsNegated) result->op = plankton::Negate(result->op);
        return nullptr;
    }
    
    antlrcpp::Any visitCondBinaryEq(PlanktonParser::CondBinaryEqContext* ctx) override { return Handle<BinaryOperator::EQ>(*ctx); }
    antlrcpp::Any visitCondBinaryNeq(PlanktonParser::CondBinaryNeqContext* ctx) override { return Handle<BinaryOperator::NEQ>(*ctx); }
    antlrcpp::Any visitCondBinaryLt(PlanktonParser::CondBinaryLtContext* ctx) override { return Handle<BinaryOperator::LT>(*ctx); }
    antlrcpp::Any visitCondBinaryLte(PlanktonParser::CondBinaryLteContext* ctx) override { return Handle<BinaryOperator::LEQ>(*ctx); }
    antlrcpp::Any visitCondBinaryGt(PlanktonParser::CondBinaryGtContext* ctx) override { return Handle<BinaryOperator::GT>(*ctx); }
    antlrcpp::Any visitCondBinaryGte(PlanktonParser::CondBinaryGteContext* ctx) override { return Handle<BinaryOperator::GEQ>(*ctx); }
};

std::unique_ptr<BinaryExpression> AstBuilder::MakeExpression(PlanktonParser::BinaryConditionContext& context) {
    BinaryConditionBuilder builder(*this);
    context.accept(&builder);
    if (builder.result) return std::move(builder.result);
    throw std::logic_error("Internal error: could not create binary expression."); // TODO: better error handling
}
