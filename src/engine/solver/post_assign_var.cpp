#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"

using namespace plankton;


PostImage Solver::Post(std::unique_ptr<Annotation> pre, const VariableAssignment& cmd) const {
    assert(cmd.lhs.size() == cmd.rhs.size());
    plankton::CheckAccess(cmd, *pre); // TODO: ensure no shared variable is updated
    plankton::InlineAndSimplify(*pre);
    
    // evaluate rhs
    SymbolFactory factory(*pre);
    std::deque<std::unique_ptr<SymbolicVariable>> symbols;
    for (const auto& expr : cmd.rhs) {
        auto value = plankton::MakeSymbolic(*expr, *pre->now);
        auto& symbol = factory.GetFreshFO(expr->Type());
        symbols.push_back(std::make_unique<SymbolicVariable>(symbol));
        pre->Conjoin(std::make_unique<StackAxiom>(
                BinaryOperator::EQ, std::make_unique<SymbolicVariable>(symbol), std::move(value)
        ));
    }
    
    // update lhs
    for (std::size_t index = 0; index < cmd.lhs.size(); ++index) {
        auto& decl = cmd.lhs.at(index)->Decl();
        assert(!decl.isShared);
        auto& resource = plankton::GetResource(decl, *pre->now);
        resource.value = std::move(symbols.at(index));
    }
    
    plankton::InlineAndSimplify(*pre);
    // TODO: extend stack? expand memory? find fulfillments?
    return PostImage(std::move(pre));
}
