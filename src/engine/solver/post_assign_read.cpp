#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"

using namespace plankton;


PostImage Solver::Post(std::unique_ptr<Annotation> pre, const MemoryRead& cmd) const {
    assert(cmd.lhs.size() == cmd.rhs.size());
    PrepareAccess(*pre, cmd);
    plankton::InlineAndSimplify(*pre);
    
    for (std::size_t index = 0; index < cmd.lhs.size(); ++index) {
        auto& resource = plankton::GetResource(cmd.lhs.at(index)->Decl(), *pre->now);
        auto& value = plankton::Evaluate(*cmd.rhs.at(index), *pre->now);
        resource.value = std::make_unique<SymbolicVariable>(value);
    }
    
    // TODO: extend stack? expand memory? find fulfillments?
    return PostImage(std::move(pre));
}
