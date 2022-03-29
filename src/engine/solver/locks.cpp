#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;


template<typename T>
inline std::unique_ptr<StackAxiom> MakeLockAssumption(const SymbolDeclaration& lock) {
    assert(lock.type.sort == Sort::TID);
    return std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(lock),
                                        std::make_unique<T>());
}

template<typename PRE, typename POST>
inline PostImage ChangeLock(std::unique_ptr<Annotation> pre, const Dereference& lockExpr, const Solver& solver) {
    // TODO: if the invariant can react on locks, then locking/unlocking must check for conserving the invariant!
    // TODO: do we need to do history / future reasoning?

    plankton::InlineAndSimplify(*pre);
    auto& lock = plankton::Evaluate(lockExpr, *pre->now);

    // ensure lock is available ~> not reentrant
    pre->now->Conjoin(MakeLockAssumption<PRE>(lock));
    if (solver.IsUnsatisfiable(*pre)) {
        DEBUG("{ false }" << std::endl << std::endl)
        return PostImage();
    }

    // acquire lock
    SymbolFactory factory(*pre);
    auto& newLock = factory.GetFreshFO(lockExpr.GetType());
    auto& memory = plankton::GetResource(plankton::Evaluate(*lockExpr.variable, *pre->now), *pre->now);
    assert(memory.fieldToValue.at(lockExpr.fieldName)->Decl() == lock);
    auto preMemory = plankton::Copy(memory);
    memory.fieldToValue[lockExpr.fieldName] = std::make_unique<SymbolicVariable>(newLock);
    pre->Conjoin(MakeLockAssumption<POST>(newLock));
    plankton::InlineAndSimplify(*pre);
    DEBUG(*pre << std::endl << std::endl)

    // make effect, if needed
    PostImage result(std::move(pre));
    if (!plankton::IsLocal(memory)) {
        auto preMem = plankton::Copy(dynamic_cast<const SharedMemoryCore&>(*preMemory));
        auto postMem = plankton::Copy(dynamic_cast<const SharedMemoryCore&>(memory));
        auto context = std::make_unique<SeparatingConjunction>();
        context->Conjoin(MakeLockAssumption<PRE>(lock));
        context->Conjoin(MakeLockAssumption<POST>(newLock)); // TODO: does this work / break things?
        result.effects.push_back(std::make_unique<HeapEffect>(std::move(preMem), std::move(postMem), std::move(context)));
    }

    return result;
}

PostImage Solver::Post(std::unique_ptr<Annotation> pre, const AcquireLock& cmd) const {
    MEASURE("Solver::Post (LOCK)")
    DEBUG("<<POST LOCK>>" << std::endl << *pre << " " << cmd << std::flush)
    PrepareAccess(*pre, cmd);
    return ChangeLock<SymbolicUnlocked, SymbolicSelfTid>(std::move(pre), *cmd.lock, *this);
}

PostImage Solver::Post(std::unique_ptr<Annotation> pre, const ReleaseLock& cmd) const {
    MEASURE("Solver::Post (UNLOCK)")
    DEBUG("<<POST UNLOCK>>" << std::endl << *pre << " " << cmd << std::flush)
    PrepareAccess(*pre, cmd);
    return ChangeLock<SymbolicSelfTid, SymbolicUnlocked>(std::move(pre), *cmd.lock, *this);
}
