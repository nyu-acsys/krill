#include "engine/solver.hpp"

#include "engine/encoding.hpp"

using namespace plankton;


bool Solver::IsUnsatisfiable(const Annotation& annotation) const {
    Encoding encoding(*annotation.now);
    encoding.AddPremise(encoding.EncodeInvariants(*annotation.now, config));
    encoding.AddPremise(encoding.EncodeSimpleFlowRules(*annotation.now, config));
    return encoding.ImpliesFalse();
}
