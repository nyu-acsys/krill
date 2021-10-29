#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;


inline bool MatchesPremise(const FuturePredicate& future, const Guard& guard, std::vector<std::unique_ptr<Dereference>>& updateLhs) {
    if (!plankton::SyntacticalEqual(guard, *future.guard)) return false;
    if (updateLhs.size() != future.update->fields.size()) return false;
    for (std::size_t index = 0; index < updateLhs.size(); ++index) {
        if (!plankton::SyntacticalEqual(*updateLhs.at(index), *future.update->fields.at(index))) return false;
    }
    return true;
}

inline bool MatchesPremise(const FutureSuggestion& suggestion, const FuturePredicate& future) {
    return MatchesPremise(future, *suggestion.guard, suggestion.command->lhs);
}

inline bool MatchesPremise(const FuturePredicate& future, const FuturePredicate& other) {
    return MatchesPremise(other, *future.guard, future.update->fields);
}


//
// Filter
//

inline bool SameUpdate(const FuturePredicate& future, const FuturePredicate& other, Encoding& encoding) {
    auto vector = plankton::MakeVector<EExpr>(future.update->values.size());
    for (std::size_t index = 0; index < vector.size(); ++index) {
        vector.push_back(encoding.Encode(*future.update->values.at(index)) == encoding.Encode(*other.update->values.at(index)));
    }
    return encoding.Implies(encoding.MakeAnd(vector));
}

inline void FilterFutures(Annotation& annotation) {
    // TODO: semantic filter
    Encoding encoding(*annotation.now);
    for (auto it = annotation.future.begin(); it != annotation.future.end(); ++it) {
        if (!*it) continue;
        const auto& future = **it;
        for (auto ot = std::next(it); ot != annotation.future.end(); ++ot) {
            if (!*ot) continue;
            auto& other = **ot;
            if (!MatchesPremise(future, other)) continue;
            if (!SameUpdate(future, other, encoding)) continue;
            ot->reset(nullptr);
        }
    }
    plankton::RemoveIf(annotation.future, [](const auto& elem){ return !elem; });
}

inline void FilterEffects(std::deque<std::unique_ptr<HeapEffect>>& effects) {
    for (auto& effect : effects) {
        SymbolFactory factory;
        auto renaming = plankton::MakeDefaultRenaming(factory);
        plankton::RenameSymbols(*effect->pre, renaming);
        plankton::RenameSymbols(*effect->post, renaming);
        plankton::RenameSymbols(*effect->context, renaming);
    }

    for (auto it = effects.begin(); it != effects.end(); ++it) {
        if (!*it) continue;
        for (auto ot = std::next(it); ot != effects.end(); ++ot) {
            if (!*ot) continue;
            if (!plankton::SyntacticalEqual(*(**it).pre, *(**ot).pre)) continue;
            if (!plankton::SyntacticalEqual(*(**it).post, *(**ot).post)) continue;
            if (!plankton::SyntacticalEqual(*(**it).context, *(**ot).context)) continue;
            ot->reset(nullptr);
        }
    }

    plankton::RemoveIf(effects, [](const auto& elem){ return !elem; });
}


//
// Find new futures
//

struct FutureInfo {
    Annotation& annotation;
    const Guard& guard;
    SymbolFactory factory;
    std::unique_ptr<Update> targetUpdate;
    std::deque<const FuturePredicate*> matchingFutures;

    explicit FutureInfo(Annotation& annotation, const Guard& guard) : annotation(annotation), guard(guard) {}
    FutureInfo(FutureInfo&& other) = default;
};

inline std::optional<FutureInfo> MakeFutureInfo(Annotation& annotation, const FutureSuggestion& suggestion) {
    DEBUG("MakeFutureInfo for " << annotation << std::endl)
    FutureInfo result(annotation, *suggestion.guard);

    // extract updates
    result.targetUpdate = std::make_unique<Update>();
    for (std::size_t index = 0; index < suggestion.command->lhs.size(); ++index) {
        auto newValue = plankton::TryMakeSymbolic(*suggestion.command->rhs.at(index), *annotation.now);
        if (!newValue) return std::nullopt;
        result.targetUpdate->fields.push_back(plankton::Copy(*suggestion.command->lhs.at(index)));
        result.targetUpdate->values.push_back(std::move(newValue));
    }

    // find matching futures
    for (const auto& future : annotation.future) {
        if (!MatchesPremise(suggestion, *future)) continue;
        result.matchingFutures.push_back(future.get());
    }

    return result;
}

inline bool TargetUpdateIsCovered(const FutureInfo& info) {
    Encoding encoding(*info.annotation.now);
    return plankton::Any(info.matchingFutures, [&info,&encoding](const auto* future) {
        for (std::size_t index = 0 ; index < future->update->values.size() ; ++index) {
            auto equal = encoding.Encode(*info.targetUpdate->values.at(index)) == encoding.Encode(*future->update->values.at(index));
            if (!encoding.Implies(equal)) return false;
        }
        return true;
    });
}

inline void AddTrivialFuture(Annotation& annotation, const FutureSuggestion& target) {
    auto update = std::make_unique<Update>();
    for (const auto& field : target.command->lhs) {
        auto value = plankton::TryMakeSymbolic(*field, *annotation.now);
        if (!value) return;
        update->fields.push_back(plankton::Copy(*field));
        update->values.push_back(std::move(value));
    }

    auto result = std::make_unique<FuturePredicate>(std::move(update), plankton::Copy(*target.guard));
    annotation.future.push_back(std::move(result));
}

inline std::unique_ptr<SeparatingConjunction> ExtractStack(const Annotation& annotation) {
    struct : public LogicListener {
        std::set<const Axiom*> axioms;
        void Visit(const EqualsToAxiom& object) override { axioms.insert(&object); }
        void Visit(const StackAxiom& object) override { axioms.insert(&object); }
        void Visit(const InflowEmptinessAxiom& object) override { axioms.insert(&object); }
        void Visit(const InflowContainsValueAxiom& object) override { axioms.insert(&object); }
        void Visit(const InflowContainsRangeAxiom& object) override { axioms.insert(&object); }
    } collector;
    annotation.now->Accept(collector);

    auto result = std::make_unique<SeparatingConjunction>();
    for (const auto* axiom : collector.axioms) result->Conjoin(plankton::Copy(*axiom));
    return result;
}

inline std::pair<const SharedMemoryCore*, const SharedMemoryCore*>
GetImmutabilityModification(const Annotation& annotation, const SymbolDeclaration& address) {
    auto now = dynamic_cast<const SharedMemoryCore*>(plankton::TryGetResource(address, *annotation.now));
    const SharedMemoryCore* past = nullptr;
    for (const auto& elem : annotation.past) {
        if (elem->formula->node->Decl() != address) continue;
        past = elem->formula.get();
        break;
    }
    if (now && past) return { now, past };
    throw std::logic_error("Internal error: failed to make stable memory."); // TODO: better error handling
}

inline std::unique_ptr<SharedMemoryCore>
MakeImmutable(const SharedMemoryCore& memory, const Formula& context, const Solver& solver, SymbolFactory& factory) {
    auto annotation = std::make_unique<Annotation>();
    annotation->Conjoin(plankton::Copy(memory));
    annotation->Conjoin(std::make_unique<PastPredicate>(plankton::Copy(memory)));
    annotation->Conjoin(plankton::Copy(context));
    plankton::Simplify(*annotation);
    annotation = solver.MakeInterferenceStable(std::move(annotation));

    // TODO: this is really ugly and fragile
    auto result = plankton::Copy(memory);
    auto [now, past] = GetImmutabilityModification(*annotation, memory.node->Decl());
    auto makeImmutable = [&factory](auto& sym){ sym->decl = factory.GetFresh(sym->Type(), sym->Order()); };
    if (now->flow->Decl() != past->flow->Decl()) makeImmutable(result->flow);
    for (const auto& [field, value] : now->fieldToValue) {
        if (value->Decl() != past->fieldToValue.at(field)->Decl()) makeImmutable(result->fieldToValue.at(field));
    }

    return result;
}

inline std::unique_ptr<SeparatingConjunction> ExtractImmutableState(FutureInfo& info, const Solver& solver) {
    auto memories = plankton::Collect<SharedMemoryCore>(*info.annotation.now);
    for (const auto& past : info.annotation.past) memories.insert(past->formula.get());

    auto stack = ExtractStack(info.annotation);
    auto result = std::make_unique<Annotation>();
    for (const auto* mem : memories) {
        result->Conjoin(MakeImmutable(*mem, *stack, solver, info.factory));
    }
    result->Conjoin(std::move(stack));

    // for (const auto* mem : memories) {
    //     auto immutable = plankton::Copy(*mem);
    //     immutable->flow->decl = info.factory.GetFreshSO(immutable->flow->Type());
    //     if (!solver.IsMemoryImmutable(*immutable, *stack)) continue;
    //     result->Conjoin(std::move(immutable));
    // }

    // plankton::InlineAndSimplify(*result);
    return std::move(result->now);
}

inline std::unique_ptr<Annotation> MakePostState(FutureInfo& info, const FuturePredicate& future,
                                                 const FutureSuggestion& target, const Type& flowType) {
    struct : public ProgramListener {
        std::set<const VariableDeclaration*> variables;
        std::set<const VariableDeclaration*> dereferences;
        void Enter(const VariableDeclaration& object) override { variables.insert(&object); }
        void Enter(const Dereference& object) override { dereferences.insert(&object.variable->Decl()); }
    } collector;
    for (const auto& elem : future.guard->conjuncts) elem->Accept(collector);
    for (const auto& elem : future.update->fields) elem->Accept(collector);
    target.command->Accept(collector);

    std::set<const EqualsToAxiom*> resources;
    std::set<const SymbolDeclaration*> addresses;
    for (const auto* var : collector.variables) {
        resources.insert(&plankton::GetResource(*var, *info.annotation.now));
    }
    for (const auto* var : collector.dereferences) {
        addresses.insert(&plankton::GetResource(*var, *info.annotation.now).Value());
    }

    auto result = std::make_unique<Annotation>();
    for (const auto* res : resources) result->Conjoin(plankton::Copy(*res));
    for (const auto* adr : addresses) result->Conjoin(plankton::MakeSharedMemory(*adr, flowType, info.factory));

    for (const auto& guard : future.guard->conjuncts) {
        auto lhs = plankton::MakeSymbolic(*guard->lhs, *result->now);
        auto rhs = plankton::MakeSymbolic(*guard->rhs, *result->now);
        result->Conjoin(std::make_unique<StackAxiom>(guard->op, std::move(lhs), std::move(rhs)));
    }

    return result;
}

PostImage Solver::ImproveFuture(std::unique_ptr<Annotation> pre, const FutureSuggestion& target) const {
    DEBUG("== Solver::ImproveFuture in " << *pre << std::endl)
    assert(target.command);
    assert(pre);

    PostImage result(std::move(pre));
    auto& annotation = *result.annotations.front();
    AddTrivialFuture(annotation, target);

    plankton::InlineAndSimplify(annotation);
    auto info = MakeFutureInfo(annotation, target);
    if (!info || TargetUpdateIsCovered(*info)) return result;

    auto immutable = ExtractImmutableState(*info, *this);
    DEBUG("  -- immutable: " << *immutable << std::endl)

    for (const auto* future : info->matchingFutures) {
        DEBUG("  -- handling future: " << *future << std::endl)
        try {
            auto check = MakePostState(*info, *future, target, config.GetFlowValueType());
            check->Conjoin(plankton::Copy(*immutable));
            plankton::InlineAndSimplify(*check);

            auto post = Post(std::move(check), *target.command, false);
            plankton::MoveInto(std::move(post.effects), result.effects);
            auto newFuture = std::make_unique<FuturePredicate>(plankton::Copy(*info->targetUpdate), plankton::Copy(*future->guard));
            DEBUG("     -- post image succeeded, adding future: " << *newFuture << std::endl)
            annotation.Conjoin(std::move(newFuture));

        } catch (std::logic_error& err) { // TODO: catch proper error
            DEBUG("     -- post image failed: " << err.what() << std::endl)
        }
    }

    FilterFutures(annotation);
    FilterEffects(result.effects);
    DEBUG("    - resulting annotation: " << annotation << std::endl)
    DEBUG("    - resulting effects: " << std::endl) for (const auto& elem : result.effects) DEBUG("        - " << *elem << std::endl)
    return result;
}
