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

inline bool ContainsMemoryFor(const SymbolDeclaration& decl, const Formula& formula) {
    struct MemChecker : public LogicListener {
        const SymbolDeclaration& search;
        bool found = false;
        explicit MemChecker(const SymbolDeclaration& search) : search(search) {}
        inline void Handle(const MemoryAxiom& object) { if (object.node->Decl() == search) found = true; }
        void Visit(const LocalMemoryResource& object) override { Handle(object); }
        void Visit(const SharedMemoryCore& object) override { Handle(object); }
    };

    MemChecker checker(decl);
    formula.Accept(checker);
    return checker.found;
}

inline bool Consumes(const FuturePredicate& consumer, const FuturePredicate& consumed, const Formula& now, Encoding& encoding) {
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> map;
    for (std::size_t index = 0; index < consumer.update->values.size(); ++index) {
        auto consumerVar = dynamic_cast<const SymbolicVariable*>(consumer.update->values.at(index).get());
        auto consumedVar = dynamic_cast<const SymbolicVariable*>(consumed.update->values.at(index).get());
        if (!consumerVar || !consumedVar) return false;
        if (ContainsMemoryFor(consumedVar->Decl(), now)) return false;
        map[&consumedVar->Decl()] = &consumerVar->Decl();
    }
    auto renaming = [&map](const auto& decl) -> const SymbolDeclaration& {
        auto find = map.find(&decl);
        if (find != map.end()) return *find->second;
        else return decl;
    };

    auto renamed = plankton::Copy(now);
    plankton::RenameSymbols(*renamed, renaming);
    return encoding.Implies(*renamed);
}

inline void FilterFutures(Annotation& annotation) {
    Encoding encoding(*annotation.now);
    for (const auto& future : annotation.future) {
        if (!future) continue;
        for (auto& other : annotation.future) {
            if (!other) continue;
            if (future.get() == other.get()) continue;
            if (!MatchesPremise(*future, *other)) continue;
            if (!Consumes(*future, *other, *annotation.now, encoding)) continue;
            other.reset(nullptr);
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

inline std::unique_ptr<StackAxiom> MakeEq(const SymbolDeclaration& decl, const SymbolicExpression& expr) {
    return std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(decl), plankton::Copy(expr));
}

inline std::unique_ptr<Annotation> MakePostState(FutureInfo& info, const FuturePredicate& future,
                                                 const FutureSuggestion& target, const Type& flowType) {
    // find variables and dereferences
    struct : public ProgramListener {
        std::set<const VariableDeclaration*> variables;
        std::set<const VariableDeclaration*> dereferences;
        void Enter(const VariableDeclaration& object) override { variables.insert(&object); }
        void Enter(const Dereference& object) override { dereferences.insert(&object.variable->Decl()); }
    } collector;
    for (const auto& elem : future.guard->conjuncts) elem->Accept(collector);
    for (const auto& elem : future.update->fields) elem->Accept(collector);
    target.command->Accept(collector);

    // find variables and dereferences
    std::set<const EqualsToAxiom*> resources;
    std::set<const SymbolDeclaration*> addresses;
    for (const auto* var : collector.variables) {
        resources.insert(&plankton::GetResource(*var, *info.annotation.now));
    }
    for (const auto* var : collector.dereferences) {
        addresses.insert(&plankton::GetResource(*var, *info.annotation.now).Value());
    }

    // make variables and dereferences accessible
    auto result = std::make_unique<Annotation>();
    for (const auto* res : resources) result->Conjoin(plankton::Copy(*res));
    for (const auto* adr : addresses) result->Conjoin(plankton::MakeSharedMemory(*adr, flowType, info.factory));

    // add guard knowledge
    for (const auto& guard : future.guard->conjuncts) {
        auto lhs = plankton::MakeSymbolic(*guard->lhs, *result->now);
        auto rhs = plankton::MakeSymbolic(*guard->rhs, *result->now);
        result->Conjoin(std::make_unique<StackAxiom>(guard->op, std::move(lhs), std::move(rhs)));
    }

    // apply updates
    for (std::size_t index = 0; index < future.update->fields.size(); ++index) {
        auto& lhs = *future.update->fields.at(index);
        auto& rhs = *future.update->values.at(index);
        auto& memory = plankton::GetResource(plankton::Evaluate(*lhs.variable, *result->now), *result->now);
        auto& value = *memory.fieldToValue.at(lhs.fieldName);
        value.decl = info.factory.GetFreshFO(value.Type());
        result->Conjoin(MakeEq(value.Decl(), rhs));
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

    for (const auto* future : info->matchingFutures) {
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
    DEBUG("    - resulting effects: " << std::endl) for (const auto& elem : result.effects) DEBUG("        - " << *elem << std::endl)
    return result;
}
