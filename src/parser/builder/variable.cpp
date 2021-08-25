#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"

using namespace plankton;

void AstBuilder::AddDecl(PlanktonParser::VarDeclContext& context, bool shared) {
    auto name = context.name->getText();
    auto typeName = MakeBaseTypeName(*context.type());
    auto& type = TypeByName(typeName);
    AddDecl(std::make_unique<VariableDeclaration>(name, type, shared));
}

void AstBuilder::AddDecl(PlanktonParser::VarDeclListContext& context, bool shared) {
    auto typeName = MakeBaseTypeName(*context.type());
    for (auto* nameToken : context.names) {
        auto name = nameToken->getText();
        auto& type = TypeByName(name);
        AddDecl(std::make_unique<VariableDeclaration>(name, type, shared));
    }
}
