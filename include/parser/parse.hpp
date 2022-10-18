#pragma once
#ifndef PLANKTON_PARSER_PARSE_HPP
#define PLANKTON_PARSER_PARSE_HPP

#include <memory>
#include <fstream>
#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/config.hpp"
#include "footprint/flowconstraint.hpp"

namespace plankton {
    
    struct ParsedSolverConfig : public SolverConfig {
        [[nodiscard]] const Type& GetFlowValueType() const override { return Type::Data(); }
        [[nodiscard]] std::size_t GetMaxFootprintDepth(const Type&, const std::string&) const override { return 1; }
    };

    struct ParsingResult {
        std::unique_ptr<Program> program;
        std::unique_ptr<ParsedSolverConfig> config;
        std::string footprintConfig;
    };

    struct FlowConstraintsParsingResult {
        std::vector<std::unique_ptr<Type>> types;
        std::unique_ptr<SolverConfig> config;
        std::deque<std::unique_ptr<FlowConstraint>> constraints;
        std::string name;
    };
    
    ParsingResult Parse(const std::string& filename, bool spuriousCasFails = true);
    ParsingResult Parse(std::istream& input, bool spuriousCasFails = true);
    
    std::unique_ptr<Program> ParseProgram(const std::string& filename, bool spuriousCasFails = true);
    std::unique_ptr<Program> ParseProgram(std::istream& input, bool spuriousCasFails = true);

    FlowConstraintsParsingResult ParseFlowConstraints(const std::string& filename);
    FlowConstraintsParsingResult ParseFlowConstraints(std::istream& input);
    
} // namespace plankton

#endif //PLANKTON_PARSER_PARSE_HPP
