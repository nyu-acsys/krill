#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"

using namespace plankton;


std::unique_ptr<Scope> AstBuilder::MakeScope(PlanktonParser::ScopeContext& context) {
    if (context.statement().empty()) return std::make_unique<Scope>(std::make_unique<Skip>());
    
    PushScope();
    for (auto* variableContext : context.varDecl()) {
        AddDecl(MakeVariable(*variableContext, false));
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
    if (builder.result) throw std::logic_error("Internal error: AST construction failed."); // TODO: better error handling
    return std::move(builder.result);
}

std::unique_ptr<Choice> AstBuilder::MakeChoice(PlanktonParser::StmtChooseContext& ctx) {
    auto result = std::make_unique<Choice>();
    for (auto* branch : ctx.scope()) result->branches.push_back(MakeScope(*branch));
    return result;
}

inline std::unique_ptr<Choice> BuildChoiceFrom(AstBuilder& builder, PlanktonParser::ConditionContext& condition,
                                               const std::function<std::unique_ptr<Scope>()>& makeTrueBranch,
                                               const std::function<std::unique_ptr<Scope>()>& makeFalseBranch) {
    // TODO: what is the semantics of branchConditions.trueCases? Conjunction or Disjunction?
    throw std::logic_error("not yet implemented");
    
    auto result = std::make_unique<Choice>();
    auto branchConditions = builder.MakeBranchConditions(condition);

    auto handle = [](const auto& makeScope, auto cond) {
        auto result = makeScope();
        auto assume = std::make_unique<Assume>(std::move(cond));
        result->body = std::make_unique<Sequence>(std::move(assume), std::move(result->body));
        return std::move(result);
    };
    auto handleAll = [&result, &handle](const auto& makeScope, auto& container) {
        for (auto&& elem : container)
            result->branches.push_back(handle(makeScope, std::move(elem)));
    };
    
    handleAll(makeTrueBranch, branchConditions.trueCases);
    handleAll(makeFalseBranch, branchConditions.falseCases);
    return result;
}

std::unique_ptr<Choice> AstBuilder::MakeChoice(PlanktonParser::StmtIfContext& ctx) {
    auto mkTrue = [this,&ctx](){ return MakeScope(*ctx.bif); };
    auto mkFalse = [this,&ctx](){ return MakeScope(*ctx.belse); };
    auto result = BuildChoiceFrom(*this, *ctx.condition(), mkTrue, mkFalse);
    assert(!result->branches.empty());
    return result;
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
    auto choice = BuildChoiceFrom(builder, *ctx.condition(), mkTrue, mkFalse);
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
    antlrcpp::Any visitCmdAssign(PlanktonParser::CmdAssignContext* ctx) override { result = builder.MakeAssignment(*ctx); return nullptr; }
    antlrcpp::Any visitCmdCond(PlanktonParser::CmdCondContext* ctx) override { result = builder.MakeAssignment(*ctx); return nullptr; }
    antlrcpp::Any visitCmdMalloc(PlanktonParser::CmdMallocContext* ctx) override { result = builder.MakeMalloc(*ctx); return nullptr; }
    antlrcpp::Any visitCmdAssume(PlanktonParser::CmdAssumeContext* ctx) override { result = builder.MakeAssume(*ctx->condition()); return nullptr; }
    antlrcpp::Any visitCmdAssert(PlanktonParser::CmdAssertContext* ctx) override { result = builder.MakeAssert(*ctx->condition()); return nullptr; }
    antlrcpp::Any visitCmdCall(PlanktonParser::CmdCallContext* ctx) override { result = builder.MakeCall(*ctx); return nullptr; }
    antlrcpp::Any visitCmdBreak(PlanktonParser::CmdBreakContext*) override { result = std::make_unique<Break>(); return nullptr; }
    antlrcpp::Any visitCmdReturnVoid(PlanktonParser::CmdReturnVoidContext* ctx) override { result = std::make_unique<Return>(); return nullptr; }
    antlrcpp::Any visitCmdReturnExpr(PlanktonParser::CmdReturnExprContext* ctx) override { result = builder.MakeReturn(*ctx); return nullptr; }
    antlrcpp::Any visitCmdCas(PlanktonParser::CmdCasContext* ctx) override { result = builder.MakeCas(*ctx); return nullptr; }
};

template<typename T>
inline std::unique_ptr<Statement> BuildStatement(T& context, AstBuilder& builder) {
    StatementBuilder visitor(builder);
    context.accept(&visitor);
    if (visitor.result) throw std::logic_error("Internal error: AST construction failed."); // TODO: better error handling
    return std::move(visitor.result);
}

std::unique_ptr<Statement> AstBuilder::MakeStatement(PlanktonParser::StatementContext& context) {
    return BuildStatement(context, *this);
}

std::unique_ptr<Statement> AstBuilder::MakeCommand(PlanktonParser::CommandContext& context) {
    return BuildStatement(context, *this);
}
