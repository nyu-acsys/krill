#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"

using namespace plankton;


std::string AstBuilder::MakeBaseTypeName(PlanktonParser::TypeContext& context) const {
    struct : public PlanktonBaseVisitor {
        antlrcpp::Any visitTypeBool(PlanktonParser::TypeBoolContext*) override { return Type::Bool().name; }
        antlrcpp::Any visitTypeInt(PlanktonParser::TypeIntContext*) override { return Type::Data().name; }
        antlrcpp::Any visitTypeData(PlanktonParser::TypeDataContext*) override { return Type::Data().name; }
        antlrcpp::Any visitTypeThread(PlanktonParser::TypeThreadContext*) override { return Type::Thread().name; }
        antlrcpp::Any visitTypePtr(PlanktonParser::TypePtrContext* ctx) override { return ctx->name->getText(); }
    } visitor;
    return std::any_cast<std::string>(context.accept(&visitor));
}
