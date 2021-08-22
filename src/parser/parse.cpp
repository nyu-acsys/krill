#include "parser/parse.hpp"

#include "parser/builder.hpp"

using namespace plankton;


ParsingResult plankton::ParseProgram(const std::string& filename) {
    std::ifstream stream(filename);
    return plankton::ParseProgram(stream);
}

ParsingResult plankton::ParseProgram(std::istream& input) {
    return AstBuilder::BuildFrom(input);
}
