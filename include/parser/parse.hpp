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
    
    ParsingResult ParseProgram(const std::string& filename);
    ParsingResult ParseProgram(std::istream& input);
    
} // namespace plankton

#endif //PLANKTON_PARSER_PARSE_HPP
