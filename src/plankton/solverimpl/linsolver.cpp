#include "plankton/solverimpl/linsolver.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


std::shared_ptr<Encoder> MakeEncoder(const PostConfig& /*config*/) {
	throw std::logic_error("not yet implemented: MakeEncoder");
}

SolverImpl::SolverImpl(PostConfig config_) : Solver(std::move(config_)), encoder(MakeEncoder(config)) {}


std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker(const heal::Formula& premise) const {
	auto checker = encoder->MakeImplicationChecker();
	checker->AddPremise(premise);
	return checker;
}

bool SolverImpl::ImpliesFalse(const heal::Formula& formula) const {
	return MakeImplicationChecker(formula)->ImpliesFalse();
}

bool SolverImpl::ImpliesFalseQuick(const heal::Formula& formula) const {
	ExpressionAxiom falseAxiom(std::make_unique<BooleanValue>(true));
	return heal::syntactically_contains_conjunct(formula, falseAxiom);
}

bool SolverImpl::Implies(const heal::Formula& formula, const heal::Formula& implied) const {
	return MakeImplicationChecker(formula)->Implies(implied);
}

bool SolverImpl::Implies(const heal::Formula& formula, const cola::Expression& implied) const {
	return MakeImplicationChecker(formula)->Implies(implied);
}

bool SolverImpl::ImpliesIsNull(const heal::Formula& formula, const cola::Expression& expression) const {
	return MakeImplicationChecker(formula)->ImpliesIsNull(expression);
}

bool SolverImpl::ImpliesIsNonNull(const heal::Formula& formula, const cola::Expression& expression) const {
	return MakeImplicationChecker(formula)->ImpliesIsNonNull(expression);
}

std::unique_ptr<heal::ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const ImplicationChecker& checker) const {
	return checker.ComputeImpliedConjuncts(GetCandidates());
}

std::unique_ptr<heal::ConjunctionFormula> SolverImpl::ComputeImpliedCandidates(const heal::Formula& premise) const {
	auto checker = MakeImplicationChecker(premise);
	checker->AddPremise(GetInstantiatedInvariant());
	return ComputeImpliedCandidates(*checker);
}


std::unique_ptr<heal::ConjunctionFormula> SolverImpl::AddInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const {
	return heal::conjoin(std::move(formula), heal::copy(GetInstantiatedInvariant()));
}

std::unique_ptr<heal::ConjunctionFormula> SolverImpl::StripInvariant(std::unique_ptr<heal::ConjunctionFormula> formula) const {
	auto& invariant = GetInstantiatedInvariant();
	return heal::remove_conjuncts_if(std::move(formula), [&invariant](const auto& conjunct){
		return heal::syntactically_contains_conjunct(invariant, conjunct);
	});
}

std::unique_ptr<heal::Annotation> SolverImpl::AddInvariant(std::unique_ptr<heal::Annotation> annotation) const {
	annotation->now = AddInvariant(std::move(annotation->now));
	return annotation;
}

std::unique_ptr<heal::Annotation> SolverImpl::StripInvariant(std::unique_ptr<heal::Annotation> annotation) const {
	annotation->now = StripInvariant(std::move(annotation->now));
	return annotation;
}
