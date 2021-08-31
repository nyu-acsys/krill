#include "engine/solver.hpp"

#include "logics/util.hpp"

using namespace plankton;


inline std::unique_ptr<Annotation>
AddScope(std::unique_ptr<Annotation> pre, const std::vector<std::unique_ptr<VariableDeclaration>>& scope) {
    // Note: avoiding clashes is not strictly necessary as the tool won't confuse the different variable
    //       declaration with the same name, however, the user will...
    // TODO: really check for clashes?
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

inline std::unique_ptr<Annotation>
RemoveScope(std::unique_ptr<Annotation> pre, const std::vector<std::unique_ptr<VariableDeclaration>>& scope) {
    if (scope.empty()) return pre;
    auto remove = plankton::Collect<EqualsToAxiom>(*pre->now, [&scope](const auto& resource){
        return Any(scope, [&resource](const auto& decl){ return resource.Variable() == *decl; });
    });
    pre->now->RemoveConjunctsIf([&remove](const auto& formula){ return Membership(remove, &formula); });
    return pre;
}

std::unique_ptr<Annotation> Solver::PostLeave(std::unique_ptr<Annotation> pre, const Function& scope) const {
    return RemoveScope(std::move(pre), scope.parameters);
}

std::unique_ptr<Annotation> Solver::PostLeave(std::unique_ptr<Annotation> pre, const Scope& scope) const {
    return RemoveScope(std::move(pre), scope.variables);
}
