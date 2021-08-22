#include "parser/builder.hpp"

#include "antlr4-runtime.h"
#include "PlanktonLexer.h"
#include "PlanktonParser.h"

using namespace plankton;


ParsingResult AstBuilder::BuildFrom(std::istream& input) {
    antlr4::ANTLRInputStream antlr(input);
    PlanktonLexer lexer(&antlr);
    antlr4::CommonTokenStream tokens(&lexer);
    
    PlanktonParser parser(&tokens);
    parser.removeErrorListeners();
    parser.setErrorHandler(std::make_shared<antlr4::BailErrorStrategy>());
    
    try {
        auto context = parser.program();
        assert(parser.getNumberOfSyntaxErrors() == 0);
        assert(context);
        AstBuilder builder;
        return { builder.MakeProgram(*context), builder.MakeConfig(*context) };

    } catch (antlr4::ParseCancellationException& e) {
        // TODO: get better error message from AntLR?
        throw std::logic_error("Parse error" + std::string(e.what()) + "."); // TODO: better error handling
    }
}
