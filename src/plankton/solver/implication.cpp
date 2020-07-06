#include "plankton/solver/solverimpl.hpp"

#include <iostream> // TODO: delete

using namespace cola;
using namespace plankton;


ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, const Formula& /*premise_*/) : encoder(encoder_), solver(encoder.MakeSolver()) {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl()");
}

bool ImplicationCheckerImpl::ImpliesFalse() const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::ImpliesFalse");
}

bool ImplicationCheckerImpl::Implies(const Formula& /*implied*/) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::Implies");
}

bool ImplicationCheckerImpl::Implies(const cola::Expression& /*implied*/) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::Implies");
}

bool ImplicationCheckerImpl::ImpliesNonNull(const cola::Expression& /*nonnull*/) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::ImpliesNonNull");
}
