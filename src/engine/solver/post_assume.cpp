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

inline bool Discard(const Formula& formula, const SolverConfig& config) {
    Encoding encoding(formula);
    encoding.AddPremise(encoding.EncodeInvariants(formula, config));
    encoding.AddPremise(encoding.EncodeSimpleFlowRules(formula, config));
    return encoding.ImpliesFalse();
}

PostImage Solver::Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const {
    MEASURE("Solver::Post (Assume)")
    DEBUG("<<POST ASSUME>>" << std::endl << *pre << " " << cmd << std::flush)
    PrepareAccess(*pre, cmd);
    plankton::InlineAndSimplify(*pre);
    pre->Conjoin(ConvertToLogic(*cmd.condition, *pre->now));
    if (Discard(*pre->now, config)) { DEBUG("{ false }" << std::endl << std::endl) return PostImage(); }
    DEBUG(*pre << std::endl << std::endl)
    return PostImage(std::move(pre));
}
