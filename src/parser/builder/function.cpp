#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"

using namespace plankton;


struct FunctionInfo : public PlanktonBaseVisitor {
    std::string name = "__init__";
    Function::Kind kind = Function::INIT;
    PlanktonParser::TypeListContext* typeList = nullptr;
    PlanktonParser::ArgDeclListContext* argDeclList = nullptr;
    PlanktonParser::ScopeContext* body = nullptr;
    
    template<typename T> void Handle(T* ctx) {
        name = ctx->name->getText();
        typeList = ctx->typeList();
        argDeclList = ctx->argDeclList();
        body = ctx->scope();
    }
    antlrcpp::Any visitFunctionInterface(PlanktonParser::FunctionInterfaceContext* ctx) override {
        kind = Function::API;
        Handle(ctx);
        return nullptr;
    }
    antlrcpp::Any visitFunctionMacro(PlanktonParser::FunctionMacroContext* ctx) override {
        kind = Function::MACRO;
        Handle(ctx);
        return nullptr;
    }
    antlrcpp::Any visitFunctionInit(PlanktonParser::FunctionInitContext* ctx) override {
        body = ctx->scope();
        return nullptr;
    }
    
};

inline std::vector<std::reference_wrapper<const Type>> MakeReturnTypes(FunctionInfo& info, AstBuilder& builder) {
    if (!info.typeList) return {};
    std::vector<std::reference_wrapper<const Type>> result;
    for (auto* type : info.typeList->types) {
        result.emplace_back(builder.TypeByName(builder.MakeBaseTypeName(*type)));
    }
    return result;
}

inline void AddParams(FunctionInfo& info, AstBuilder& builder) {
    if (!info.argDeclList) return;
    for (auto* decl : info.argDeclList->args) {
        builder.AddDecl(builder.MakeVariable(*decl, false));
    }
}

std::unique_ptr<Function> AstBuilder::MakeFunction(PlanktonParser::FunctionContext& context) {
    FunctionInfo info;
    context.accept(&info);
    assert(info.body);
    assert(info.kind == Function::INIT || (info.typeList && info.argDeclList && info.body));
    
    PushScope();
    AddParams(info, *this);
    
    auto function = std::make_unique<Function>(info.name, info.kind, MakeScope(*info.body));
    function->returnType = MakeReturnTypes(info, *this);
    function->parameters = PopScope();

    return function;
}
