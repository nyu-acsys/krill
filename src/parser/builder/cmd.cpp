#include "parser/builder.hpp"

#include "programs/util.hpp"

using namespace plankton;


template<typename T, typename U>
inline std::vector<std::unique_ptr<T>> As(U& context, AstBuilder& builder) {
    std::vector<std::unique_ptr<T>> result;
    for (auto* exprContext : context.expression()) {
        auto expr = builder.MakeExpression(*exprContext);
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

template<typename L, typename R>
std::unique_ptr<Statement> BuildMemoryAssignment(AstBuilder& builder, const L& lhsContainer, const R& rhsContainer) {
    auto result = std::make_unique<MemoryWrite>();
    for (std::size_t index = 0; index < lhsContainer.size(); ++index) {
    
    }
    
    throw std::logic_error("not yet implemented");
}

template<typename L, typename R>
std::unique_ptr<Statement> BuildStackAssignment(AstBuilder& builder, const L& lhsContainer, const R& rhsContainer) {
    throw std::logic_error("not yet implemented");
}

template<typename L, typename R>
std::unique_ptr<Statement> BuildAssignment(AstBuilder& builder, const L& lhsContainer, const R& rhsContainer) {
    if (lhsContainer.size() != rhsContainer.size()) {
        throw std::logic_error("Parse error: unbalanced assignment."); // TODO: better error handling
    }
    bool isWrite = plankton::All(lhsContainer, [](auto& elem){ return plankton::IsOfType<Dereference>(*elem).first; });
    bool isStack = plankton::All(lhsContainer, [](auto& elem){ return plankton::IsOfType<VariableExpression>(*elem).first; });
    if (isWrite && !isStack) return BuildMemoryAssignment(builder, lhsContainer, rhsContainer);
    else if (isStack && !isWrite) return BuildStackAssignment(builder, lhsContainer, rhsContainer);
    throw std::logic_error("Parse error: left-hand side of assignments must be homogeneously either variables or dereferences."); // TODO: better error handling
}

std::unique_ptr<Statement> AstBuilder::MakeAssignment(PlanktonParser::CmdAssignContext& ctx) {
    auto lhs = As<ValueExpression>(*ctx.lhs, *this);
    auto rhs = As<ValueExpression>(*ctx.lhs, *this);
    return BuildAssignment(*this, lhs, rhs);
}

std::unique_ptr<Statement> AstBuilder::MakeAssignment(PlanktonParser::CmdCondContext& ctx) {
    throw std::logic_error("Parse error: unsupported assignment from condition, desugar using if-then-else instead.'"); // TODO: better error handling
}

std::unique_ptr<Statement> AstBuilder::MakeAssume(PlanktonParser::ConditionContext& ctx);
std::unique_ptr<Statement> AstBuilder::MakeAssert(PlanktonParser::ConditionContext& ctx);

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

std::unique_ptr<Malloc> AstBuilder::MakeMalloc(PlanktonParser::CmdMallocContext& ctx) {
    auto& var = VariableByName(ctx.lhs->getText());
    return std::make_unique<Malloc>(std::make_unique<VariableExpression>(var));
}

std::unique_ptr<Statement> AstBuilder::MakeReturn(PlanktonParser::CmdReturnExprContext& ctx) {
    auto result = std::make_unique<Return>();
    result->expressions = As<SimpleExpression>(*ctx.exprList(), *this);
    return result;
}

std::unique_ptr<Statement> AstBuilder::MakeCas(PlanktonParser::CmdCasContext& ctx) {
    auto dst = As<ValueExpression>(*ctx.cas()->dst, *this);
    auto cmp = As<SimpleExpression>(*ctx.cas()->cmp, *this);
    auto src = As<SimpleExpression>(*ctx.cas()->src, *this);
    if (dst.size() != cmp.size() && dst.size() != src.size()) {
        throw std::logic_error("Parse error: unbalanced Compare-and-Swap."); // TODO: better error handling
    }
    if (dst.empty()) return std::make_unique<Skip>();
    
    auto choice = std::make_unique<Choice>();
    auto mkAssume = [&dst,&cmp](std::size_t index, BinaryOperator op){
        auto lhs = plankton::Copy(*dst.at(index));
        auto rhs = plankton::Copy(*cmp.at(index));
        auto cond = std::make_unique<BinaryExpression>(op, std::move(lhs), std::move(rhs));
        return std::make_unique<Assume>(std::move(cond));
    };
    
    // true branch
    std::unique_ptr<Statement> success;
    for (std::size_t index = 0; index < dst.size(); ++index) {
        auto assume = mkAssume(index, BinaryOperator::EQ);
        if (!success) success = std::move(assume);
        else success = std::make_unique<Sequence>(std::move(success), std::move(assume));
    }
    auto assign = BuildAssignment(*this, dst, src);
    success = std::make_unique<Sequence>(std::move(success), std::move(assign));
    choice->branches.front()->body = std::move(success);
    
    // false branches
    for (std::size_t index = 0; index < cmp.size(); ++index) {
        choice->branches.push_back(std::make_unique<Scope>(mkAssume(index, BinaryOperator::NEQ)));
    }
    
    return std::make_unique<Atomic>(std::make_unique<Scope>(std::move(choice)));
}
