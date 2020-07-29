#include "plankton/solverimpl/linsolver.hpp"

#include "plankton/backend/z3encoder.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


std::shared_ptr<Encoder> MakeEncoder(const PostConfig& config) {
	return std::make_shared<Z3Encoder>(config);
}

SolverImpl::SolverImpl(PostConfig config_) : Solver(std::move(config_)), encoder(MakeEncoder(config)) {}


std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker() const {
	 return encoder->MakeImplicationChecker();
}

std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker(EncodingTag tag) const {
	return encoder->MakeImplicationChecker(tag);
}

std::unique_ptr<ImplicationChecker> SolverImpl::MakeImplicationChecker(const heal::Formula& premise) const {
	auto checker = encoder->MakeImplicationChecker();
	checker->AddPremise(premise);
	return checker;
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
