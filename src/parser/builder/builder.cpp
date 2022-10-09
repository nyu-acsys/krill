#include "parser/builder.hpp"

#include <sstream>
#include "antlr4-runtime.h"
#include "PlanktonLexer.h"
#include "PlanktonParser.h"

using namespace plankton;


AstBuilder::AstBuilder(bool spuriousCasFails) : spuriousCasFails(spuriousCasFails) {}

bool AstBuilder::SpuriousCasFail() const { return spuriousCasFails; }

struct ParseErrorListener : public antlr4::BaseErrorListener {
    void syntaxError(antlr4::Recognizer* /*recognizer*/, antlr4::Token* /*offendingSymbol*/, size_t line,
                     size_t charPositionInLine, const std::string& msg, std::exception_ptr /*err*/) override {
        std::stringstream stream;
        stream << "syntax error at line " << line << ":" << charPositionInLine << "; " << msg << std::endl;
        throw antlr4::ParseCancellationException(stream.str()); // TODO: better error handling
    }
};

ParsingResult AstBuilder::BuildFrom(std::istream& input, bool spuriousCasFail) {
    antlr4::ANTLRInputStream antlr(input);
    PlanktonLexer lexer(&antlr);
    antlr4::CommonTokenStream tokens(&lexer);
    
    PlanktonParser parser(&tokens);
    ParseErrorListener errorListener;
    parser.removeErrorListeners();
    parser.addErrorListener(&errorListener);
    // parser.setErrorHandler(std::make_shared<antlr4::BailErrorStrategy>());
    
    try {
        auto context = parser.program();
        assert(parser.getNumberOfSyntaxErrors() == 0);
        assert(context);
        AstBuilder builder(spuriousCasFail);
        ParsingResult result;
        builder.PrepareMake(*context);
        result.config = builder.MakeConfig(*context);
        result.program = builder.MakeProgram(*context);
        result.footprintConfig = builder.MakeFootprintConfig(*result.program, *context);
        return result;

    } catch (antlr4::ParseCancellationException& e) {
        // TODO: get better error message from AntLR?
        throw std::logic_error("Parse error: " + std::string(e.what()) + "."); // TODO: better error handling
    }
}

FlowConstraintsParsingResult AstBuilder::BuildGraphsFrom(std::istream& input) {
    antlr4::ANTLRInputStream antlr(input);
    PlanktonLexer lexer(&antlr);
    antlr4::CommonTokenStream tokens(&lexer);

    PlanktonParser parser(&tokens);
    ParseErrorListener errorListener;
    parser.removeErrorListeners();
    parser.addErrorListener(&errorListener);

    try {
        auto context = parser.flowgraphs();
        assert(parser.getNumberOfSyntaxErrors() == 0);
        assert(context);
        AstBuilder builder(true);
        FlowConstraintsParsingResult result;
        builder.PrepareMake(*context);
        return builder.MakeFlowgraphs(*context);

    } catch (antlr4::ParseCancellationException& e) {
        // TODO: get better error message from AntLR?
        throw std::logic_error("Parse error: " + std::string(e.what()) + "."); // TODO: better error handling
    }
}