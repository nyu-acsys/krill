#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"

using namespace plankton;


inline void CheckAllocation(const LocalMemoryResource& memory, const Formula& state, const SolverConfig& config) {
    // TODO: check invariant
    throw std::logic_error("not yet implemented");
    
//    z3::context context;
//    z3::solver solver(context);
//    Z3Encoder encoder(context, solver);
//    auto good = engine::ComputeImplied(solver, encoder(state), encoder(*config.GetNodeInvariant(*memory)));
//    if (!good)
//        throw std::logic_error("Newly allocated node does not satisfy invariant."); // TODO: better error handling
}

inline std::unique_ptr<LocalMemoryResource>
MakeMemoryResource(const SymbolicVariable& address, SymbolFactory& factory, const SolverConfig& config) {
    auto node = plankton::Copy(address);
    auto flow = std::make_unique<SymbolicVariable>(factory.GetFreshSO(config.GetFlowValueType()));
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    for (const auto&[field, type] : address.Type().fields) {
        auto& value = factory.GetFreshFO(type);
        fieldToValue[field] = std::make_unique<SymbolicVariable>(value);
    }
    return std::make_unique<LocalMemoryResource>(std::move(node), std::move(flow), std::move(fieldToValue));
}

inline std::pair<std::unique_ptr<SymbolicVariable>, std::unique_ptr<Formula>>
MakeFreshCell(const Type& type, const Annotation& frame, const SolverConfig& config) {
    SymbolFactory factory(frame);
    auto address = std::make_unique<SymbolicVariable>(factory.GetFreshFO(type));
    auto memory = MakeMemoryResource(*address, factory, config);
    auto context = std::make_unique<SeparatingConjunction>();
    
    // address is not NULL
    context->Conjoin(std::make_unique<StackAxiom>(
            BinaryOperator::NEQ, plankton::Copy(*address), std::make_unique<SymbolicNull>(type)
    ));
    
    // address has no inflow
    context->Conjoin(std::make_unique<InflowEmptinessAxiom>(memory->flow->Decl(), true));
    
    // pointer fields at address are NULL
    for (const auto&[field, value] : memory->fieldToValue) {
        if (value->Sort() != Sort::PTR) continue;
        context->Conjoin(std::make_unique<StackAxiom>(
                BinaryOperator::EQ, plankton::Copy(*value), std::make_unique<SymbolicNull>(type)
        ));
    }
    
    // address is fresh, i.e., distinct from other addresses
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
    plankton::CheckAccess(cmd, *pre);
    plankton::InlineAndSimplify(*pre);
    assert(!cmd.lhs->Decl().isShared); // TODO: ensure via CheckAccess // TODO: throw?
    
    auto& resource = plankton::GetResource(cmd.lhs->Decl(), *pre->now);
    auto [cell, context] = MakeFreshCell(cmd.lhs->Type(), *pre, config);
    resource.value = std::move(cell);
    pre->Conjoin(std::move(context));
    return PostImage(std::move(pre));
}
