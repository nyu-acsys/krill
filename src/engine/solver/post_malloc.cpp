#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/timer.hpp"

using namespace plankton;


inline void CheckAllocation(const LocalMemoryResource& memory, const Formula& state, const SolverConfig& config) {
    auto invariant = config.GetLocalNodeInvariant(memory);
    if (Encoding(state).Implies(*invariant)) return;
    throw std::logic_error("Newly allocated node does not satisfy invariant."); // TODO: better error handling
}

inline std::pair<std::unique_ptr<SymbolicVariable>, std::unique_ptr<Formula>>
MakeFreshCell(const Type& type, const Annotation& frame, const SolverConfig& config) {
    SymbolFactory factory(frame);
    auto address = std::make_unique<SymbolicVariable>(factory.GetFreshFO(type));
    auto memory = plankton::MakeLocalMemory(address->Decl(), config.GetFlowValueType(), factory);
    auto context = std::make_unique<SeparatingConjunction>();
    
    // address is not NULL
    context->Conjoin(std::make_unique<StackAxiom>(
            BinaryOperator::NEQ, plankton::Copy(*address), std::make_unique<SymbolicNull>()
    ));
    
    // address has no inflow
    context->Conjoin(std::make_unique<InflowEmptinessAxiom>(memory->flow->Decl(), true));
    
    // pointer fields at address are NULL
    for (const auto&[field, value] : memory->fieldToValue) {
        if (value->GetSort() != Sort::PTR) continue;
        context->Conjoin(std::make_unique<StackAxiom>(
                BinaryOperator::EQ, plankton::Copy(*value), std::make_unique<SymbolicNull>()
        ));
    }
    
    // address is fresh, i.e., distinct from other addresses
    // TODO: not true when havoc is supported for pointers
    auto hasSameType = [&type](const auto& sym) { return sym.order == Order::FIRST && sym.type == type; };
    for (const auto* symbol : Collect<SymbolDeclaration>(frame, hasSameType)) {
        context->Conjoin(std::make_unique<StackAxiom>(
                BinaryOperator::NEQ, plankton::Copy(*address), std::make_unique<SymbolicVariable>(*symbol)
        ));
    }
    
    CheckAllocation(*memory, *context, config);
    context->Conjoin(std::move(memory));
    return {std::move(address), std::move(context)};
}


PostImage Solver::Post(std::unique_ptr<Annotation> pre, const Malloc& cmd) const {
    MEASURE("Solver::Post (Malloc)")
    PrepareAccess(*pre, cmd);
    plankton::InlineAndSimplify(*pre);
    assert(!cmd.lhs->Decl().isShared);
    
    auto& resource = plankton::GetResource(cmd.lhs->Decl(), *pre->now);
    auto [cell, context] = MakeFreshCell(cmd.lhs->GetType(), *pre, config);
    resource.value = std::move(cell);
    pre->Conjoin(std::move(context));
    return PostImage(std::move(pre));
}
