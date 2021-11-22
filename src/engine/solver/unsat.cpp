#include "engine/solver.hpp"

#include "engine/encoding.hpp"

using namespace plankton;


bool Solver::IsUnsatisfiable(const Annotation& annotation) const {
    return Encoding(*annotation.now, config).ImpliesFalse();
}
