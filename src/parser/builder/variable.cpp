#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"

using namespace plankton;

std::unique_ptr<VariableDeclaration> AstBuilder::MakeVariable(PlanktonParser::VarDeclContext& context, bool shared) {
    auto name = context.name->getText();
    auto typeName = MakeBaseTypeName(*context.type());
    auto& type = TypeByName(typeName);
    return std::make_unique<VariableDeclaration>(name, type, shared);
}
