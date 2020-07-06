#include "plankton/solver/solverimpl.hpp"

#include <iostream> // TODO: delete

using namespace cola;
using namespace plankton;


std::unique_ptr<Annotation> SolverImpl::Join(std::vector<std::unique_ptr<Annotation>> /*annotations*/) const {
	// TODO: strip invariant, join, then add invariant
	throw std::logic_error("not yet implemented: SolverImpl::Join");
}
