#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"
#include "util/shortcuts.hpp"

using namespace plankton;


template<typename T>
struct ContextCollector : public PlanktonBaseVisitor {
    std::vector<T*> result;
    
    template<typename U>
    antlrcpp::Any Handle(U* /*context*/) { /* do nothing */ return nullptr; }
    template<>
    antlrcpp::Any Handle<T>(T* context) { if (context) result.push_back(context); return nullptr; }
    
    antlrcpp::Any visitStructDecl(PlanktonParser::StructDeclContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitProgramVariable(PlanktonParser::ProgramVariableContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitFunctionInterface(PlanktonParser::FunctionInterfaceContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitFunctionMacro(PlanktonParser::FunctionMacroContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitFunctionInit(PlanktonParser::FunctionInitContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitProgramContains(PlanktonParser::ProgramContainsContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitProgramOutflow(PlanktonParser::ProgramOutflowContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitProgramInvariantVariable(PlanktonParser::ProgramInvariantVariableContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitProgramInvariantNode(PlanktonParser::ProgramInvariantNodeContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitOption(PlanktonParser::OptionContext* ctx) override { return Handle(ctx); }
};

template<typename T>
inline std::vector<T*> Elements(PlanktonParser::ProgramContext& context) {
    ContextCollector<T> collector;
    context.accept(&collector);
    return std::move(collector.result);
}

inline void HandleTypes(PlanktonParser::ProgramContext& context, AstBuilder& builder) {
    auto typeContexts = Elements<PlanktonParser::StructDeclContext>(context);

    // initialize types without fields
    std::vector<Type*> types;
    for (auto* typeContext : typeContexts) {
        auto name = typeContext->name->getText();
        if (name == "void") {
            throw std::logic_error("Parse error: 'void' not allowed as struct name."); // TODO: better error handling
        }
        if (builder.TypeByNameOrNull(name)) {
            throw std::logic_error("Parse error: redefinition of type name '" + name + "'."); // TODO: better error handling
        }
        auto newType = std::make_unique<Type>(name, Sort::PTR);
        types.push_back(newType.get());
        builder.AddDecl(std::move(newType));
    }
    
    // link fields
    for (std::size_t index = 0; index < types.size(); ++index) {
        for (auto* fieldContext : typeContexts.at(index)->fieldDecl()) {
            auto fieldName = fieldContext->name->getText();
            auto typeName = builder.MakeBaseTypeName(*fieldContext->type());
            auto& type = builder.TypeByName(typeName);
            auto insertion = types.at(index)->fields.emplace(fieldName, type);
            if (insertion.second) continue;
            throw std::logic_error("Parsing error: redefinition of field '" + fieldName + "' in type '" + typeName + "'."); // TODO: better error handling
        }
    }
}

inline void HandleSharedVariables(PlanktonParser::ProgramContext& context, AstBuilder& builder) {
    for (auto* variable : Elements<PlanktonParser::ProgramVariableContext>(context)) {
        builder.AddDecl(*variable->varDeclList(), true);
    }
}

template<typename T>
inline void HandleFunctions(PlanktonParser::ProgramContext& context, AstBuilder& builder) {
    for (auto* function : Elements<T>(context)) {
        builder.AddDecl(builder.MakeFunction(*function));
    }
}

std::unique_ptr<Function> MakeInitFunction(PlanktonParser::ProgramContext& context, AstBuilder& builder) {
    auto functionContexts = Elements<PlanktonParser::FunctionInitContext>(context);
    if (functionContexts.empty())
        throw std::logic_error("Parse error: missing '__init__' function."); // TODO: better error handling
    if (functionContexts.size() != 1)
        throw std::logic_error("Parse error: function '__init__' multiply defined."); // TODO: better error handling
    return builder.MakeFunction(*functionContexts.front());
}

std::string MakeName(PlanktonParser::ProgramContext& context, AstBuilder& /*builder*/) {
    for (auto* optionContext : Elements<PlanktonParser::OptionContext>(context)) {
        if (optionContext->ident->getText() == "name") return optionContext->str->getText();
    }
    return "Untitled Program";
}

std::unique_ptr<Program> AstBuilder::MakeProgram(PlanktonParser::ProgramContext& context) {
    PushScope();
    
    HandleTypes(context, *this);
    HandleSharedVariables(context, *this);
    HandleFunctions<PlanktonParser::FunctionMacroContext>(context, *this);
    HandleFunctions<PlanktonParser::FunctionInterfaceContext>(context, *this);
    
    auto init = MakeInitFunction(context, *this);
    auto name = MakeName(context, *this);
    auto program = std::make_unique<Program>(name, std::move(init));
    // TODO: remove skips
    // TODO: apply left movers
    
    program->variables = PopScope();
    plankton::MoveInto(std::move(_types), program->types);
    for (auto&& function : std::move(_functions)) {
        switch (function->kind) {
            case Function::API: program->apiFunctions.push_back(std::move(function)); break;
            case Function::MACRO: program->macroFunctions.push_back(std::move(function)); break;
            case Function::INIT: throw std::logic_error("Internal error: unexpected function type."); // TODO: better error handling
        }
    }
    
    return program;
}
