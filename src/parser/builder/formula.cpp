#include "parser/builder.hpp"

using namespace plankton;


std::unique_ptr<Formula> AstBuilder::MakeFormula(PlanktonParser::FormulaContext& context, const Formula& eval) {
    throw std::logic_error("not yet implemented");
}

std::unique_ptr<Invariant> AstBuilder::MakeInvariant(PlanktonParser::InvariantContext& context, const Formula& eval) {
    throw std::logic_error("not yet implemented");
}