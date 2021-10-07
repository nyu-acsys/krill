#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/timer.hpp"

using namespace plankton;


inline std::unique_ptr<StackAxiom> ConvertToLogic(const BinaryExpression& condition, const Formula& context) {
    return std::make_unique<StackAxiom>(condition.op, plankton::MakeSymbolic(*condition.lhs, context),
                                        plankton::MakeSymbolic(*condition.rhs, context));
}

PostImage Solver::Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const {
    MEASURE("Solver::Post (Assume)")
    PrepareAccess(*pre, cmd);
    plankton::InlineAndSimplify(*pre);
    pre->Conjoin(ConvertToLogic(*cmd.condition, *pre->now));
    if (Encoding(*pre->now).ImpliesFalse()) return PostImage();
    return PostImage(std::move(pre));
}
