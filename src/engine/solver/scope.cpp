#include "engine/solver.hpp"

#include "logics/util.hpp"

using namespace plankton;


inline std::unique_ptr<Annotation>
AddScope(std::unique_ptr<Annotation> pre, const std::vector<std::unique_ptr<VariableDeclaration>>& scope) {
    auto inScope = plankton::Collect<EqualsToAxiom>(*pre->now);
    auto clash = Any(scope, [&inScope](const auto& elem){
        return ContainsIf(inScope, [&elem](auto* other){ return other->Variable().name == elem->name; });
    });
    if (clash) throw std::logic_error("Variable hiding is not supported."); // TODO: better error handling
    
    SymbolFactory factory(*pre);
    for (const auto& variable : scope) {
        auto& symbol = factory.GetFreshFO(variable->type);
        pre->Conjoin(std::make_unique<EqualsToAxiom>(*variable, symbol));
    }
    return pre;
}

std::unique_ptr<Annotation> Solver::PostEnter(std::unique_ptr<Annotation> pre, const Program& scope) const {
    return AddScope(std::move(pre), scope.variables);
}

std::unique_ptr<Annotation> Solver::PostEnter(std::unique_ptr<Annotation> pre, const Function& scope) const {
    return AddScope(std::move(pre), scope.parameters);
}

std::unique_ptr<Annotation> Solver::PostEnter(std::unique_ptr<Annotation> pre, const Scope& scope) const {
    return AddScope(std::move(pre), scope.variables);
}

inline std::set<const VariableDeclaration*> CollectVariables(const FuturePredicate& future) {
    struct : public ProgramListener {
        std::set<const VariableDeclaration*> result;
        void Enter(const VariableDeclaration& object) override { result.insert(&object); }
    } collector;
    for (const auto& elem : future.guard->conjuncts) elem->Accept(collector);
    for (const auto& elem : future.update->fields) elem->Accept(collector);
    return std::move(collector.result);
}

inline std::unique_ptr<Annotation>
RemoveScope(std::unique_ptr<Annotation> pre, const std::vector<std::unique_ptr<VariableDeclaration>>& scope) {
    if (scope.empty()) return pre;
    auto remove = plankton::Collect<EqualsToAxiom>(*pre->now, [&scope](const auto& resource){
        return plankton::Any(scope, [&resource](const auto& decl){ return resource.Variable() == *decl; });
    });
    pre->now->RemoveConjunctsIf([&remove](const auto& formula){ return plankton::Membership(remove, &formula); });
    plankton::RemoveIf(pre->future, [&scope](const auto& future){
        auto vars = CollectVariables(*future);
        return plankton::Any(scope, [&vars](const auto& decl){ return plankton::Membership(vars, decl.get()); });
    });
    return pre;
}

std::unique_ptr<Annotation> Solver::PostLeave(std::unique_ptr<Annotation> pre, const Function& scope) const {
    return RemoveScope(std::move(pre), scope.parameters);
}

std::unique_ptr<Annotation> Solver::PostLeave(std::unique_ptr<Annotation> pre, const Scope& scope) const {
    return RemoveScope(std::move(pre), scope.variables);
}
