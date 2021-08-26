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
        static ParsingResult BuildFrom(std::istream& input, bool spuriousCasFails); // may return NULL config
        
        // declarations (lookup / creation)
        const Type& TypeByName(const std::string& name);
        const Type* TypeByNameOrNull(const std::string& name);
        const VariableDeclaration& VariableByName(const std::string& name);
        const VariableDeclaration* VariableByNameOrNull(const std::string& name);
        const Function& FunctionByName(const std::string& name);
        const Function* FunctionByNameOrNull(const std::string& name);
        void AddDecl(std::unique_ptr<Type> type);
        void AddDecl(std::unique_ptr<VariableDeclaration> variable);
        void AddDecl(std::unique_ptr<Function> function);
        void AddDecl(PlanktonParser::VarDeclContext& context, bool shared);
        void AddDecl(PlanktonParser::VarDeclListContext& context, bool shared);
        
        // program
        std::unique_ptr<Program> MakeProgram(PlanktonParser::ProgramContext& context);
        std::unique_ptr<Function> MakeFunction(PlanktonParser::FunctionContext& context);
        std::string MakeBaseTypeName(PlanktonParser::TypeContext& context);
        
        // config
        std::unique_ptr<SolverConfig> MakeConfig(PlanktonParser::ProgramContext& context);
        // std::unique_ptr<Formula> MakeContains(PlanktonParser::ContainsPredicateContext& context);
        // std::unique_ptr<Formula> MakeOutflow(PlanktonParser::OutflowPredicateContext& context);
        // VariableInvariantGenerator MakeInvariant(PlanktonParser::VariableInvariantContext& context);
        // LocalInvariantGenerator MakeInvariant(PlanktonParser::InvLocalContext& context);
        // SharedInvariantGenerator MakeInvariant(PlanktonParser::InvSharedContext& context);
        
        // expressions
        std::unique_ptr<ValueExpression> MakeExpression(PlanktonParser::ExpressionContext& context);
        std::unique_ptr<Dereference> MakeExpression(PlanktonParser::DerefContext& context);
        std::unique_ptr<SimpleExpression> MakeExpression(PlanktonParser::ValueContext& context);
        std::unique_ptr<SimpleExpression> MakeExpression(PlanktonParser::SimpleExpressionContext& context);
        std::pair<std::unique_ptr<ValueExpression>, bool> MakeExpression(PlanktonParser::SimpleConditionContext& context);
        std::unique_ptr<BinaryExpression> MakeExpression(PlanktonParser::BinaryConditionContext& context);
        
        // statements / commands
        std::unique_ptr<Scope> MakeScope(PlanktonParser::ScopeContext& context);
        std::unique_ptr<Scope> MakeScope(PlanktonParser::BlockContext& context);
        std::unique_ptr<Statement> MakeStatement(PlanktonParser::StatementContext& context);
        std::unique_ptr<Statement> MakeCommand(PlanktonParser::CommandContext& context);
        std::unique_ptr<Statement> MakeChoice(PlanktonParser::StmtChooseContext& context);
        std::unique_ptr<Statement> MakeChoice(PlanktonParser::StmtIfContext& context);
        std::unique_ptr<UnconditionalLoop> MakeLoop(PlanktonParser::StmtLoopContext& context);
        std::unique_ptr<UnconditionalLoop> MakeLoop(PlanktonParser::StmtWhileContext& context);
        std::unique_ptr<UnconditionalLoop> MakeLoop(PlanktonParser::StmtDoContext& context);
        std::unique_ptr<Atomic> MakeAtomic(PlanktonParser::StmtAtomicContext& context);
        std::unique_ptr<Statement> MakeAssignment(PlanktonParser::CmdAssignStackContext& context);
        std::unique_ptr<Statement> MakeAssignment(PlanktonParser::CmdAssignHeapContext& context);
        std::unique_ptr<Statement> MakeAssignment(PlanktonParser::CmdAssignConditionContext& context);
        std::unique_ptr<Statement> MakeMalloc(PlanktonParser::CmdMallocContext& context);
        std::unique_ptr<Statement> MakeAssume(PlanktonParser::LogicConditionContext& context);
        std::unique_ptr<Statement> MakeAssert(PlanktonParser::LogicConditionContext& context);
        std::unique_ptr<Statement> MakeCall(PlanktonParser::CmdCallContext& context);
        std::unique_ptr<Statement> MakeReturn(PlanktonParser::CmdReturnListContext& context);
        std::unique_ptr<Statement> MakeReturn(PlanktonParser::CmdReturnExprContext& context);
        std::unique_ptr<Statement> MakeCas(PlanktonParser::CmdCasContext& context);
        
        // parsing configuration
        bool SpuriousCasFail() const;

        private:
            bool spuriousCasFails;
            std::deque<std::unique_ptr<Type>> _types;
            std::deque<std::unique_ptr<Function>> _functions;
            std::deque<std::deque<std::unique_ptr<VariableDeclaration>>> _variables;
            
            explicit AstBuilder(bool spuriousCasFails);
            void PushScope();
            std::vector<std::unique_ptr<VariableDeclaration>> PopScope();
    };
    
}

#endif //PLANKTON_PARSER_BUILDER_HPP
