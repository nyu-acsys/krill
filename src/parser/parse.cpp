#include "parser/parse.hpp"

#include "parser/builder.hpp"

using namespace plankton;

inline ParsingResult GetParsingResult(std::istream& input, bool spuriousCasFails) {
    auto result = AstBuilder::BuildFrom(input, spuriousCasFails);
    if (result.program) return result;
    throw std::logic_error("Internal error: failed to parse program");
}

ParsingResult plankton::Parse(std::istream& input, bool spuriousCasFails) {
    auto result = GetParsingResult(input, spuriousCasFails);
    if (result.config) return result;
    throw std::logic_error("Parse error: no configuration found."); // TODO: better error handling
}

ParsingResult plankton::Parse(const std::string& filename, bool spuriousCasFails) {
    std::ifstream stream(filename);
    return plankton::Parse(stream, spuriousCasFails);
}

std::unique_ptr<Program> plankton::ParseProgram(std::istream& input, bool spuriousCasFails) {
    return GetParsingResult(input, spuriousCasFails).program;
}

std::unique_ptr<Program> plankton::ParseProgram(const std::string& filename, bool spuriousCasFails) {
    std::ifstream stream(filename);
    return ParseProgram(stream, spuriousCasFails);
}
