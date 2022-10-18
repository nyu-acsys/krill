#include <utility>

#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"
#include "util/shortcuts.hpp"

using namespace plankton;


inline void HandleTypes(const std::vector<PlanktonParser::StructDeclContext*>& typeContexts, AstBuilder& builder) {
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
    for (auto* varDeclList : context.vars) {
        builder.AddDecl(*varDeclList, true);
    }
}

template<typename T>
struct FunctionContextCollector : public PlanktonBaseVisitor {
    std::vector<T*> result;

    template<typename U>
    antlrcpp::Any Handle(U* context) {
        if constexpr (std::is_base_of_v<T, U>) {
            if (context) result.push_back(context);
        }
        return nullptr;
    }
    // template<typename U>
    // antlrcpp::Any Handle(U* /*context*/) { /* do nothing */ return nullptr; }
    // template<>
    // antlrcpp::Any Handle<T>(T* context) { if (context) result.push_back(context); return nullptr; }
    
    antlrcpp::Any visitFunctionInterface(PlanktonParser::FunctionInterfaceContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitFunctionMacro(PlanktonParser::FunctionMacroContext* ctx) override { return Handle(ctx); }
    antlrcpp::Any visitFunctionInit(PlanktonParser::FunctionInitContext* ctx) override { return Handle(ctx); }
};

template<typename T>
inline std::vector<T*> GetFunctions(PlanktonParser::ProgramContext& context) {
    FunctionContextCollector<T> collector;
    for (auto* functionContext : context.funcs) functionContext->accept(&collector);
    return std::move(collector.result);
}

template<typename T>
inline void HandleFunctions(PlanktonParser::ProgramContext& context, AstBuilder& builder) {
    for (auto* function : GetFunctions<T>(context)) {
        builder.AddDecl(builder.MakeFunction(*function));
    }
}

std::unique_ptr<Function> MakeInitFunction(PlanktonParser::ProgramContext& context, AstBuilder& builder) {
    auto functionContexts = GetFunctions<PlanktonParser::FunctionInitContext>(context);
    if (functionContexts.empty())
        throw std::logic_error("Parse error: missing '__init__' function."); // TODO: better error handling
    if (functionContexts.size() != 1)
        throw std::logic_error("Parse error: function '__init__' multiply defined."); // TODO: better error handling
    return builder.MakeFunction(*functionContexts.front());
}

std::string MakeName(PlanktonParser::ProgramContext& context, AstBuilder& /*builder*/) {
    for (auto* optionContext : context.option()) {
        if (optionContext->ident->getText() == "name") return optionContext->str->getText();
    }
    return "Untitled Program";
}

void AstBuilder::PrepareMake(PlanktonParser::ProgramContext& context) {
    PushScope();
    HandleTypes(context.structs, *this);
    HandleSharedVariables(context, *this);
    prepared = true;
}

void AstBuilder::PrepareMake(PlanktonParser::FlowgraphsContext& context) {
    PushScope();
    HandleTypes(context.structs, *this);
    prepared = true;
}

struct VariableRenameVisitor : public MutableProgramListener {
    std::string suffix;
    explicit VariableRenameVisitor(std::string suffix) : suffix(std::move(suffix)) {}
    void Enter(VariableDeclaration& object) override { object.name += suffix; }
};

inline void PostProcess(Program& program) {
    // TODO: remove skips
    // TODO: apply left movers
    
    // rename macro variables to not clash with other functions
    for (std::size_t index = 0; index < program.macroFunctions.size(); ++index) {
        VariableRenameVisitor renaming("_" + std::to_string(index + 1));
        program.macroFunctions.at(index)->Accept(renaming);
    }
}

std::unique_ptr<Program> AstBuilder::MakeProgram(PlanktonParser::ProgramContext& context) {
    if (!prepared) throw std::logic_error("Parse error: 'AstBuilder::PrepareMake' must be called first."); // TODO: better error handling

    HandleFunctions<PlanktonParser::FunctionMacroContext>(context, *this);
    HandleFunctions<PlanktonParser::FunctionInterfaceContext>(context, *this);

    auto init = MakeInitFunction(context, *this);
    auto name = MakeName(context, *this);
    auto program = std::make_unique<Program>(name, std::move(init));

    program->variables = PopScope();
    plankton::MoveInto(std::move(_types), program->types);
    for (auto&& function : std::move(_functions)) {
        switch (function->kind) {
            case Function::API: program->apiFunctions.push_back(std::move(function)); break;
            case Function::MACRO: program->macroFunctions.push_back(std::move(function)); break;
            case Function::INIT: throw std::logic_error("Internal error: unexpected function type."); // TODO: better error handling
        }
    }

    PostProcess(*program);
    return program;
}
