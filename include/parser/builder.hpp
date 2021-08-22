#pragma once
#ifndef PLANKTON_PARSER_BUILDER_HPP
#define PLANKTON_PARSER_BUILDER_HPP

#include <memory>
#include "PlanktonParser.h"
#include "parser/parse.hpp"
#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/config.hpp"

namespace plankton {
    
    struct AstBuilder {
        static ParsingResult BuildFrom(std::istream& input);
        
        // lookup
        const Type& TypeByName(const std::string& name);
        const Type* TypeByNameOrNull(const std::string& name);
        const VariableDeclaration& VariableByName(const std::string& name);
        const VariableDeclaration* VariableByNameOrNull(const std::string& name);
        const Function& FunctionByName(const std::string& name);
        const Function* FunctionByNameOrNull(const std::string& name);
        void AddDecl(std::unique_ptr<Type> type);
        void AddDecl(std::unique_ptr<VariableDeclaration> variable);
        void AddDecl(std::unique_ptr<Function> function);
        
        // top level / general purpose
        std::unique_ptr<Program> MakeProgram(PlanktonParser::ProgramContext& context);
        std::unique_ptr<SolverConfig> MakeConfig(PlanktonParser::ProgramContext& context);
        std::unique_ptr<Function> MakeFunction(PlanktonParser::FunctionContext& context);
        std::unique_ptr<VariableDeclaration> MakeVariable(PlanktonParser::VarDeclContext& context, bool shared);
        std::string MakeBaseTypeName(PlanktonParser::TypeContext& context);
        
        // expressions
        std::unique_ptr<ValueExpression> MakeExpression(PlanktonParser::ExpressionContext& context);
        std::unique_ptr<Expression> MakeExpression(PlanktonParser::ConditionContext& context);
        std::unique_ptr<Expression> MakeExpression(PlanktonParser::SimpleConditionContext& context);
        std::unique_ptr<Expression> MakeExpression(PlanktonParser::BinaryConditionContext& context);
        std::unique_ptr<Expression> MakeExpression(PlanktonParser::LogicConditionContext& context);
        
        // resolving complicated expressions
        struct BranchConditions {
            std::deque<std::unique_ptr<BinaryExpression>> trueCases, falseCases;
        };
        BranchConditions MakeBranchConditions(PlanktonParser::ConditionContext& ctx);
        
        // statements
        std::unique_ptr<Scope> MakeScope(PlanktonParser::ScopeContext& context);
        std::unique_ptr<Scope> MakeScope(PlanktonParser::BlockContext& context);
        std::unique_ptr<Statement> MakeStatement(PlanktonParser::StatementContext& context);
        std::unique_ptr<Statement> MakeCommand(PlanktonParser::CommandContext& context);
        std::unique_ptr<Choice> MakeChoice(PlanktonParser::StmtChooseContext& ctx);
        std::unique_ptr<Choice> MakeChoice(PlanktonParser::StmtIfContext& ctx);
        std::unique_ptr<UnconditionalLoop> MakeLoop(PlanktonParser::StmtLoopContext& ctx);
        std::unique_ptr<UnconditionalLoop> MakeLoop(PlanktonParser::StmtWhileContext& ctx);
        std::unique_ptr<UnconditionalLoop> MakeLoop(PlanktonParser::StmtDoContext& ctx);
        std::unique_ptr<Atomic> MakeAtomic(PlanktonParser::StmtAtomicContext& ctx);
        std::unique_ptr<Statement> MakeAssignment(PlanktonParser::CmdAssignContext& ctx);
        std::unique_ptr<Statement> MakeAssignment(PlanktonParser::CmdCondContext& ctx);
        std::unique_ptr<Malloc> MakeMalloc(PlanktonParser::CmdMallocContext& ctx);
        std::unique_ptr<Statement> MakeAssume(PlanktonParser::ConditionContext& ctx);
        std::unique_ptr<Statement> MakeAssert(PlanktonParser::ConditionContext& ctx);
        std::unique_ptr<Statement> MakeCall(PlanktonParser::CmdCallContext& ctx);
        std::unique_ptr<Statement> MakeReturn(PlanktonParser::CmdReturnExprContext& ctx);
        std::unique_ptr<Statement> MakeCas(PlanktonParser::CmdCasContext& ctx);
        
        //        std::unique_ptr<Formula> MakeContains(PlanktonParser::ContainsPredicateContext& context);
        //        std::unique_ptr<Formula> MakeOutflow(PlanktonParser::OutflowPredicateContext& context);
        //        VariableInvariantGenerator MakeInvariant(PlanktonParser::VariableInvariantContext& context);
        //        LocalInvariantGenerator MakeInvariant(PlanktonParser::InvLocalContext& context);
        //        SharedInvariantGenerator MakeInvariant(PlanktonParser::InvSharedContext& context);

        private:
            std::deque<std::unique_ptr<Type>> _types;
            std::deque<std::unique_ptr<Function>> _functions;
            std::deque<std::deque<std::unique_ptr<VariableDeclaration>>> _variables;
            
            void PushScope();
            std::vector<std::unique_ptr<VariableDeclaration>> PopScope();
    };
    
}

#endif //PLANKTON_PARSER_BUILDER_HPP
