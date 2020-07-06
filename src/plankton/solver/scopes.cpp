#include "plankton/solver/solverimpl.hpp"

#include <iostream> // TODO: delete

using namespace cola;
using namespace plankton;


std::unique_ptr<ConjunctionFormula> make_candidates(const VariableDeclaration& /*decl*/, const std::set<const VariableDeclaration*>& /*others*/) {
	throw std::logic_error("not yet implemented: make_candidates");
}

void SolverImpl::PushScope() {
	auto instantiation = instantiatedInvariants.empty() 
		? std::make_unique<ConjunctionFormula>()
		: plankton::copy(*instantiatedInvariants.top());
	instantiatedInvariants.push(std::move(instantiation));

	auto candidates = candidateFormulas.empty()
		? std::make_unique<ConjunctionFormula>()
		: plankton::copy(*candidateFormulas.top());
	candidateFormulas.push(std::move(candidates));

	auto variables = variablesInScope.empty()
		? std::set<const VariableDeclaration*>()
		: std::set<const VariableDeclaration*>(variablesInScope.top());
	variablesInScope.push(std::move(variables));
}

void SolverImpl::ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars) {
	assert(!instantiatedInvariants.empty());
	assert(!candidateFormulas.empty());
	assert(!variablesInScope.empty());

	for (const auto& decl : vars) {
		instantiatedInvariants.top() = plankton::conjoin(std::move(instantiatedInvariants.top()), config.invariant->instantiate(*decl));
		
		auto new_candidates = make_candidates(*decl, variablesInScope.top());
		candidateFormulas.top() = plankton::conjoin(std::move(candidateFormulas.top()), std::move(new_candidates));

		variablesInScope.top().insert(decl.get());
	}
}

void SolverImpl::EnterScope(const cola::Scope& scope) {
	PushScope();
	ExtendCurrentScope(scope.variables);
}

void SolverImpl::EnterScope(const cola::Function& function) {
	PushScope();
	ExtendCurrentScope(function.args);
	ExtendCurrentScope(function.returns);
}

void SolverImpl::EnterScope(const cola::Program& program) {
	PushScope();
	ExtendCurrentScope(program.variables);
}

void SolverImpl::LeaveScope() {
	instantiatedInvariants.pop();
	candidateFormulas.pop();
	variablesInScope.pop();
}
