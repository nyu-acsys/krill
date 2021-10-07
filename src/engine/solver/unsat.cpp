#include "engine/solver.hpp"

#include "engine/encoding.hpp"

using namespace plankton;


bool Solver::IsUnsatisfiable(const Annotation& annotation) const {
    Encoding encoding(*annotation.now);
    return encoding.ImpliesFalse();
}
