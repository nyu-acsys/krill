#include "default_solver.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


std::unique_ptr<heal::Annotation> DefaultSolver::MakeStable(std::unique_ptr<heal::Annotation> pre,
                                                            const Effect& interference) const {
    // TODO: implement
    throw std::logic_error("not yet implemented: DefaultSolver::MakeStable");
}
