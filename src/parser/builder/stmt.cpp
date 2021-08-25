#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"
#include "programs/util.hpp"
#include "logics/util.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


//
// Helpers
//

inline std::unique_ptr<SimpleExpression> MakeBool(bool value) {
    if (value) return std::make_unique<TrueValue>();
    else return std::make_unique<FalseValue>();
}

inline std::unique_ptr<BinaryExpression> Negate(std::unique_ptr<BinaryExpression> expr) {
    expr->op = plankton::Negate(expr->op);
    return expr;
}

inline std::unique_ptr<BinaryExpression> Negate(const BinaryExpression& expr) {
    return Negate(plankton::Copy(expr));
}

template<typename T>
T NegateAll(T container) {
    for (auto& elem : container) elem = Negate(std::move(elem));
    return container;
}


//
// Casting
//

template<typename T>
inline std::unique_ptr<Expression> MakeExpression(T& context, AstBuilder& builder) {
    return builder.MakeExpression(context);
}
template<>
inline std::unique_ptr<Expression> MakeExpression<antlr4::Token>(antlr4::Token& context, AstBuilder& builder) {
    return std::make_unique<VariableExpression>(builder.VariableByName(context.getText()));
}

template<typename T, typename U>
inline std::vector<std::unique_ptr<T>> As(U& context, AstBuilder& builder) {
    std::vector<std::unique_ptr<T>> result;
    for (auto* exprContext : context.elems) {
        auto expr = MakeExpression(*exprContext, builder);
        if (auto cast = dynamic_cast<T*>(expr.get())) {
            result.emplace_back(cast);
            (void) expr.release(); // ownership already transferred to result
            continue;
        }
        throw std::logic_error("Parse error: unexpected expression '" + exprContext->getText() + "'."); // TODO: better error handling
    }
    return result;
}

template<typename T, typename U>
inline const T& As(const U& object) {
    if (auto cast = dynamic_cast<const T*>(&object)) {
        return *cast;
    }
    throw std::logic_error("Parse error: unexpected expression '" + plankton::ToString(object) + "'."); // TODO: better error handling
}


//
// Condition desugaring
//

struct BranchedCondition {
    std::deque<std::unique_ptr<Scope>> trueBranches, falseBranches;
    
    inline void Conjoin(std::vector<std::unique_ptr<BinaryExpression>>&& conditions, bool intoTrue) {
        std::unique_ptr<Statement> stmt;
        for (auto&& elem : conditions) {
            auto assume = std::make_unique<Assume>(std::move(elem));
            if (!stmt) stmt = std::move(assume);
            else stmt = std::make_unique<Sequence>(std::move(stmt), std::move(assume));
        }
        if (!stmt) return;
        
        auto& target = intoTrue ? trueBranches : falseBranches;
        target.push_back(std::make_unique<Scope>(std::move(stmt)));
    }
    
    inline void Disjoin(std::vector<std::unique_ptr<BinaryExpression>>&& conditions, bool intoTrue) {
        auto& target = intoTrue ? trueBranches : falseBranches;
        for (auto&& elem : conditions) {
            target.push_back(std::make_unique<Scope>(std::make_unique<Assume>(std::move(elem))));
        }
    }
};

struct ConditionBuilder : public PlanktonBaseVisitor {
    AstBuilder& builder;
    BranchedCondition result;
    
    explicit ConditionBuilder(AstBuilder& builder) : builder(builder) {}
    
    antlrcpp::Any visitCondLogicBinary(PlanktonParser::CondLogicBinaryContext* ctx) override {
        auto expr = builder.MakeExpression(*ctx->binaryCondition());
        auto neg = Negate(*expr);
        result.trueBranches.push_back(std::make_unique<Scope>(std::make_unique<Assume>(std::move(expr))));
        result.falseBranches.push_back(std::make_unique<Scope>(std::make_unique<Assume>(std::move(neg))));
        return nullptr;
    }
    
    template<bool IsConjunction, typename T>
    void HandleLogic(T& context) {
        auto trueExpr = As<BinaryExpression>(context, builder);
        auto falseExpr = NegateAll(As<BinaryExpression>(context, builder));
        
        if constexpr (IsConjunction) {
            result.Conjoin(std::move(trueExpr), true);
            result.Disjoin(std::move(falseExpr), false);
        } else {
            result.Disjoin(std::move(trueExpr), true);
            result.Conjoin(std::move(falseExpr), false);
        }
    }
    
    antlrcpp::Any visitCondLogicAnd(PlanktonParser::CondLogicAndContext* ctx) override {
        HandleLogic<true>(*ctx);
        return nullptr;
    }
    
    antlrcpp::Any visitCondLogicOr(PlanktonParser::CondLogicOrContext* ctx) override {
        HandleLogic<false>(*ctx);
        return nullptr;
    }
    
    template<typename L, typename R, typename T>
    void HandleCAS(T& context) {
        auto lhs = As<L>(*context->dst, builder);
        auto rhs = As<R>(*context->cmp, builder);
        if (lhs.size() != rhs.size())
            throw std::logic_error("Parse error: unbalanced CAS."); // TODO: better error handling
        
            auto makeExpr = [&lhs, &rhs](std::size_t index, BinaryOperator op) {
                auto lhsExpr = plankton::Copy(*lhs.at(index));
                auto rhsExpr = plankton::Copy(*rhs.at(index));
                return std::make_unique<BinaryExpression>(op, std::move(lhsExpr), std::move(rhsExpr));
            };
        
            auto trueExpr = plankton::MakeVector<std::unique_ptr<BinaryExpression>>(lhs.size());
            auto falseExpr = plankton::MakeVector<std::unique_ptr<BinaryExpression>>(lhs.size());
            for (std::size_t index = 0; index < lhs.size(); ++index) {
                trueExpr.push_back(makeExpr(index, BinaryOperator::EQ));
                falseExpr.push_back(makeExpr(index, BinaryOperator::NEQ));
            }
        
            result.Conjoin(std::move(trueExpr), true);
            result.Disjoin(std::move(falseExpr), false);
    }
    
    antlrcpp::Any visitCasStack(PlanktonParser::CasStackContext* ctx) override {
        HandleCAS<VariableExpression, ValueExpression>(ctx);
        return nullptr;
    }

    antlrcpp::Any visitCasHeap(PlanktonParser::CasHeapContext* ctx) override {
        HandleCAS<Dereference, SimpleExpression>(ctx);
        return nullptr;
    }
};

template<typename T, typename = std::enable_if_t<std::is_base_of_v<PlanktonParser::ConditionContext, T>
                                                 || std::is_base_of_v<PlanktonParser::LogicConditionContext, T>
                                                 || std::is_base_of_v<PlanktonParser::CasContext, T>>>
inline BranchedCondition DesugarCondition(AstBuilder& builder, T& context) {
    ConditionBuilder visitor(builder);
    context.accept(&visitor);
    return std::move(visitor.result);
}

template<typename T>
inline std::unique_ptr<Choice> DesugarCondition(AstBuilder& builder, T& context,
                                                const std::function<std::unique_ptr<Statement>()>& makeTrueBranch,
                                                const std::function<std::unique_ptr<Statement>()>& makeFalseBranch) {
    auto branches = DesugarCondition(builder, context);
    
    auto appendToBranch = [](auto& branch, auto stmt) {
        if (!stmt) return;
        branch->body = std::make_unique<Sequence>(std::move(branch->body), std::move(stmt));
    };
    for (auto& branch : branches.trueBranches) appendToBranch(branch, makeTrueBranch());
    for (auto& branch : branches.falseBranches) appendToBranch(branch, makeFalseBranch());
    
    auto result = std::make_unique<Choice>();
    plankton::MoveInto(std::move(branches.trueBranches), result->branches);
    plankton::MoveInto(std::move(branches.falseBranches), result->branches);
    
    if (!result->branches.empty()) return result;
    throw std::logic_error("Internal error: failed to handle conditions");
}


//
// Statements
//

std::unique_ptr<Scope> AstBuilder::MakeScope(PlanktonParser::ScopeContext& context) {
    if (context.statement().empty()) return std::make_unique<Scope>(std::make_unique<Skip>());
    
    PushScope();
    for (auto* variableContext : context.varDeclList()) {
        AddDecl(*variableContext, false);
    }
    
    std::unique_ptr<Statement> body;
    for (auto* stmtContext : context.statement()) {
        auto stmt = MakeStatement(*stmtContext);
        if (body) body = std::make_unique<Sequence>(std::move(body), std::move(stmt));
        else body = std::move(stmt);
    }
    
    assert(body);
    auto result = std::make_unique<Scope>(std::move(body));
    result->variables = PopScope();
    return result;
}

struct ScopeBuilder : PlanktonBaseVisitor {
    AstBuilder& builder;
    std::unique_ptr<Scope> result;
    explicit ScopeBuilder(AstBuilder& builder) : builder(builder) {}
    
    antlrcpp::Any visitBlockStmt(PlanktonParser::BlockStmtContext* ctx) override {
        result = std::make_unique<Scope>(builder.MakeStatement(*ctx->statement()));
        return nullptr;
    }
    antlrcpp::Any visitBlockScope(PlanktonParser::BlockScopeContext *ctx) override {
        result = builder.MakeScope(*ctx->scope());
        return nullptr;
    }
};

std::unique_ptr<Scope> AstBuilder::MakeScope(PlanktonParser::BlockContext& context) {
    ScopeBuilder builder(*this);
    context.accept(&builder);
    if (builder.result) return std::move(builder.result);
    throw std::logic_error("Internal error: AST construction failed."); // TODO: better error handling
}

std::unique_ptr<Choice> AstBuilder::MakeChoice(PlanktonParser::StmtChooseContext& ctx) {
    auto result = std::make_unique<Choice>();
    for (auto* branch : ctx.scope()) result->branches.push_back(MakeScope(*branch));
    return result;
}

std::unique_ptr<Choice> AstBuilder::MakeChoice(PlanktonParser::StmtIfContext& ctx) {
    auto mkTrue = [this,&ctx](){ return MakeScope(*ctx.bif); };
    auto mkFalse = [this,&ctx](){ return MakeScope(*ctx.belse); };
    return DesugarCondition(*this, *ctx.condition(), mkTrue, mkFalse);
}

std::unique_ptr<UnconditionalLoop> AstBuilder::MakeLoop(PlanktonParser::StmtLoopContext& ctx) {
    auto body = MakeScope(*ctx.scope());
    return std::make_unique<UnconditionalLoop>(std::move(body));
}

template<typename T>
std::unique_ptr<UnconditionalLoop> BuildConditionalLoop(T& ctx, AstBuilder& builder, bool condFirst) {
    auto body = builder.MakeScope(*ctx.body);
    auto mkTrue = [](){ return std::make_unique<Scope>(std::make_unique<Skip>()); };
    auto mkFalse = [](){ return std::make_unique<Scope>(std::make_unique<Break>()); };
    auto choice =  DesugarCondition(builder, *ctx.logicCondition(), mkTrue, mkFalse);
    if (condFirst) body->body = std::make_unique<Sequence>(std::move(choice), std::move(body->body));
    else body->body = std::make_unique<Sequence>(std::move(body->body), std::move(choice));
    return std::make_unique<UnconditionalLoop>(std::move(body));
}

std::unique_ptr<UnconditionalLoop> AstBuilder::MakeLoop(PlanktonParser::StmtWhileContext& ctx) {
    return BuildConditionalLoop(ctx, *this, true);
}

std::unique_ptr<UnconditionalLoop> AstBuilder::MakeLoop(PlanktonParser::StmtDoContext& ctx) {
    return BuildConditionalLoop(ctx, *this, false);
}

std::unique_ptr<Atomic> AstBuilder::MakeAtomic(PlanktonParser::StmtAtomicContext& ctx) {
    return std::make_unique<Atomic>(MakeScope(*ctx.block()));
}

struct StatementBuilder : public PlanktonBaseVisitor {
    AstBuilder& builder;
    std::unique_ptr<Statement> result;
    explicit StatementBuilder(AstBuilder& builder) : builder(builder) {}
    
    antlrcpp::Any visitStmtChoose(PlanktonParser::StmtChooseContext* ctx) override { result = builder.MakeChoice(*ctx); return nullptr; }
    antlrcpp::Any visitStmtLoop(PlanktonParser::StmtLoopContext* ctx) override { result = builder.MakeLoop(*ctx); return nullptr; }
    antlrcpp::Any visitStmtAtomic(PlanktonParser::StmtAtomicContext* ctx) override { result = builder.MakeAtomic(*ctx); return nullptr; }
    antlrcpp::Any visitStmtIf(PlanktonParser::StmtIfContext* ctx) override { result = builder.MakeChoice(*ctx); return nullptr; }
    antlrcpp::Any visitStmtWhile(PlanktonParser::StmtWhileContext* ctx) override { result = builder.MakeLoop(*ctx); return nullptr; }
    antlrcpp::Any visitStmtDo(PlanktonParser::StmtDoContext* ctx) override { result = builder.MakeLoop(*ctx); return nullptr; }
    antlrcpp::Any visitStmtCom(PlanktonParser::StmtComContext* ctx) override { result = builder.MakeCommand(*ctx->command()); return nullptr; }
    antlrcpp::Any visitCmdSkip(PlanktonParser::CmdSkipContext*) override { result = std::make_unique<Skip>(); return nullptr; }
    antlrcpp::Any visitCmdAssignStack(PlanktonParser::CmdAssignStackContext* ctx) override { result = builder.MakeAssignment(*ctx); return nullptr; }
    antlrcpp::Any visitCmdAssignHeap(PlanktonParser::CmdAssignHeapContext* ctx) override { result = builder.MakeAssignment(*ctx); return nullptr; }
    antlrcpp::Any visitCmdAssignCondition(PlanktonParser::CmdAssignConditionContext* ctx) override { result = builder.MakeAssignment(*ctx); return nullptr; }
    antlrcpp::Any visitCmdMalloc(PlanktonParser::CmdMallocContext* ctx) override { result = builder.MakeMalloc(*ctx); return nullptr; }
    antlrcpp::Any visitCmdAssume(PlanktonParser::CmdAssumeContext* ctx) override { result = builder.MakeAssume(*ctx->logicCondition()); return nullptr; }
    antlrcpp::Any visitCmdAssert(PlanktonParser::CmdAssertContext* ctx) override { result = builder.MakeAssert(*ctx->logicCondition()); return nullptr; }
    antlrcpp::Any visitCmdCall(PlanktonParser::CmdCallContext* ctx) override { result = builder.MakeCall(*ctx); return nullptr; }
    antlrcpp::Any visitCmdBreak(PlanktonParser::CmdBreakContext*) override { result = std::make_unique<Break>(); return nullptr; }
    antlrcpp::Any visitCmdReturnVoid(PlanktonParser::CmdReturnVoidContext* /*ctx*/) override { result = std::make_unique<Return>(); return nullptr; }
    antlrcpp::Any visitCmdReturnExpr(PlanktonParser::CmdReturnExprContext* ctx) override { result = builder.MakeReturn(*ctx); return nullptr; }
    antlrcpp::Any visitCmdCas(PlanktonParser::CmdCasContext* ctx) override { result = builder.MakeCas(*ctx); return nullptr; }
};

template<typename T>
inline std::unique_ptr<Statement> BuildStatement(T& context, AstBuilder& builder) {
    StatementBuilder visitor(builder);
    context.accept(&visitor);
    if (visitor.result) return std::move(visitor.result);
    throw std::logic_error("Internal error: AST construction failed."); // TODO: better error handling
}

std::unique_ptr<Statement> AstBuilder::MakeStatement(PlanktonParser::StatementContext& context) {
    return BuildStatement(context, *this);
}

std::unique_ptr<Statement> AstBuilder::MakeCommand(PlanktonParser::CommandContext& context) {
    return BuildStatement(context, *this);
}


//
// Commands
//

template<typename T, typename L, typename R, typename U, typename V>
inline std::unique_ptr<T> BuildAssignment(AstBuilder& builder, U& lhsContext, V& rhsContainer) {
    // build
    auto result = std::make_unique<T>();
    result->lhs = As<L>(lhsContext, builder);
    result->rhs = As<R>(rhsContainer, builder);
    
    // check
    if (result->lhs.size() != result->rhs.size()) {
        throw std::logic_error("Parse error: unbalanced assignment."); // TODO: better error handling
    }
    for (std::size_t index = 0; index < result->lhs.size(); ++index) {
        // TODO: what about as rhs NULL?
        if (result->lhs.at(index)->Type().AssignableTo(result->rhs.at(index)->Type())) continue;
        throw std::logic_error("Parse error: assignment from incompatible type."); // TODO: better error handling
    }
    
    return result;
}

template<typename L, typename R>
inline std::unique_ptr<Statement> BuildMemoryAssignment(AstBuilder& builder, L& lhsContext, R& rhsContext) {
    return BuildAssignment<MemoryWrite, Dereference, SimpleExpression>(builder, lhsContext, rhsContext);
}

template<typename L, typename R>
inline std::unique_ptr<Statement> BuildStackAssignment(AstBuilder& builder, L& lhsContext, R& rhsContext) {
    return BuildAssignment<VariableAssignment, VariableExpression, ValueExpression>(builder, lhsContext, rhsContext);
}

std::unique_ptr<Statement> AstBuilder::MakeAssignment(PlanktonParser::CmdAssignStackContext& ctx) {
    return BuildMemoryAssignment(*this, *ctx.lhs, *ctx.rhs);
}

std::unique_ptr<Statement> AstBuilder::MakeAssignment(PlanktonParser::CmdAssignHeapContext& ctx) {
    return BuildStackAssignment(*this, *ctx.lhs, *ctx.rhs);
}

std::unique_ptr<Statement> AstBuilder::MakeAssignment(PlanktonParser::CmdAssignConditionContext& ctx) {
    auto& lhs = VariableByName(ctx.lhs->getText());
    auto makeAssignment = [&lhs](bool value) {
        return std::make_unique<VariableAssignment>(std::make_unique<VariableExpression>(lhs), MakeBool(value));
    };
    return DesugarCondition(*this, *ctx.rhs,
                            [&makeAssignment]() { return makeAssignment(true); },
                            [&makeAssignment]() { return makeAssignment(false); });
}

std::unique_ptr<Statement> AstBuilder::MakeAssert(PlanktonParser::LogicConditionContext& ctx) {
    return DesugarCondition(*this, ctx, []() { return nullptr; },
                            []() { return std::make_unique<Fail>(); });
}

std::unique_ptr<Statement> AstBuilder::MakeAssume(PlanktonParser::LogicConditionContext& ctx) {
    // just take desugared true branches
    auto branches = DesugarCondition(*this, ctx);
    auto result = std::make_unique<Choice>();
    plankton::MoveInto(std::move(branches.trueBranches), result->branches);
    return result;
}

std::unique_ptr<Statement> AstBuilder::MakeCall(PlanktonParser::CmdCallContext& ctx) {
    auto& function = FunctionByName(ctx.name->getText());
    if (function.kind != Function::MACRO) {
        throw std::logic_error("Parse error: call to non-macro function '" + function.name + "' not allowed."); // TODO: better error handling
    }
    auto result = std::make_unique<Macro>(function);
    result->lhs = As<VariableExpression>(*ctx.lhs, *this);
    result->arguments = As<SimpleExpression>(*ctx.args, *this);
    return result;
}

std::unique_ptr<Statement> AstBuilder::MakeMalloc(PlanktonParser::CmdMallocContext& ctx) {
    auto& var = VariableByName(ctx.lhs->getText());
    return std::make_unique<Malloc>(std::make_unique<VariableExpression>(var));
}

std::unique_ptr<Statement> AstBuilder::MakeReturn(PlanktonParser::CmdReturnExprContext& ctx) {
    auto result = std::make_unique<Return>();
    result->expressions = As<SimpleExpression>(*ctx.varList(), *this);
    return result;
}

struct CasAssignmentBuilder : public PlanktonBaseVisitor {
    AstBuilder& builder;
    std::unique_ptr<Statement> result;
    
    explicit CasAssignmentBuilder(AstBuilder& builder) : builder(builder) {}
    
    antlrcpp::Any visitCasStack(PlanktonParser::CasStackContext* ctx) override {
        result = BuildStackAssignment(builder, *ctx->dst, *ctx->src);
        return nullptr;
    }
    
    antlrcpp::Any visitCasHeap(PlanktonParser::CasHeapContext* ctx) override {
        result = BuildMemoryAssignment(builder, *ctx->dst, *ctx->src);
        return nullptr;
    }
};

inline std::unique_ptr<Statement> MakeCasAssignment(AstBuilder& builder, PlanktonParser::CasContext& ctx) {
    CasAssignmentBuilder visitor(builder);
    ctx.accept(&visitor);
    if (visitor.result) return std::move(visitor.result);
    throw std::logic_error("Internal error: could not handle CAS."); // TODO: better error handling
}

inline std::unique_ptr<Statement> MakeStatusAssignment(AstBuilder& builder, PlanktonParser::CmdCasContext& ctx, bool value) {
    if (!ctx.lhs) return nullptr;
    auto lhs = std::make_unique<VariableExpression>(builder.VariableByName(ctx.lhs->getText()));
    return std::make_unique<VariableAssignment>(std::move(lhs), MakeBool(value));
}

std::unique_ptr<Statement> AstBuilder::MakeCas(PlanktonParser::CmdCasContext& ctx) {
    auto makeTrue = [this, &ctx]() -> std::unique_ptr<Statement> {
        auto status = MakeStatusAssignment(*this, ctx, true);
        auto assign = MakeCasAssignment(*this, *ctx.cas());
        if (!status) return assign;
        return std::make_unique<Sequence>(std::move(assign), std::move(status));
    };
    auto makeFalse = [this, &ctx](){ return MakeStatusAssignment(*this, ctx, false); };
    return DesugarCondition(*this, *ctx.cas(), makeTrue, makeFalse);
}
