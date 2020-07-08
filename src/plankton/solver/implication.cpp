#include "plankton/solver/solverimpl.hpp"

#include <iostream> // TODO: delete
#include "plankton/config.hpp"
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, const Formula& premise_) : encoder(encoder_), solver(encoder.MakeSolver()) {
	// TODO: should we store 'premise' and do some purely syntactical quick checks, or simply rely on z3?
	solver.add(encoder.Encode(premise_));
}

bool ImplicationCheckerImpl::Implies(z3::expr expr) const {
	// add negated 'expr' and check satisfiability
	solver.push();
	solver.add(!expr);
	auto result = solver.check();
	solver.pop();

	// analyse result
	switch (result) {
		case z3::unknown:
			if (plankton::config.z3_handle_unknown_result) return false;
			throw SolvingError("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");
		case z3::sat:
			return false;
		case z3::unsat:
			return true;
	}
}

bool ImplicationCheckerImpl::ImpliesFalse() const {
	return Implies(encoder.MakeFalse());
}

bool ImplicationCheckerImpl::Implies(const Formula& implied) const {
	return Implies(encoder.Encode(implied));
}

bool ImplicationCheckerImpl::Implies(const cola::Expression& implied) const {
	if (implied.sort() != Sort::BOOL) {
		throw SolvingError("Cannot check implication for expression: expression is not of boolean sort.");
	}
	return Implies(encoder.Encode(implied));
}

bool ImplicationCheckerImpl::ImpliesNonNull(const cola::Expression& nonnull) const {
	return Implies(encoder.Encode(nonnull) != encoder.MakeNullPtr());
}
