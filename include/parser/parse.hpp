#pragma once
#ifndef PLANKTON_PARSER_PARSE_HPP
#define PLANKTON_PARSER_PARSE_HPP

#include <memory>
#include <fstream>
#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/config.hpp"

namespace plankton {
    
    struct ParsingResult {
        std::unique_ptr<Program> program;
        std::unique_ptr<SolverConfig> config;
    };
    
    ParsingResult Parse(const std::string& filename, bool spuriousCasFails = true);
    ParsingResult Parse(std::istream& input, bool spuriousCasFails = true);
    
    std::unique_ptr<Program> ParseProgram(const std::string& filename, bool spuriousCasFails = true);
    std::unique_ptr<Program> ParseProgram(std::istream& input, bool spuriousCasFails = true);
    
} // namespace plankton

#endif //PLANKTON_PARSER_PARSE_HPP
