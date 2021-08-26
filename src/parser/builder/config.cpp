#include "parser/builder.hpp"

using namespace plankton;


std::unique_ptr<SolverConfig> AstBuilder::MakeConfig(PlanktonParser::ProgramContext& context) {
    // must produce a valid config, or null if none is given
    return nullptr;
}
