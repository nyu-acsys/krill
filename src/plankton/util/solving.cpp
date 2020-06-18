#include "plankton/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


bool plankton::implies_nonnull(const Formula& formula, const cola::Expression& expr) {
	// TODO: avoid copying expr (super ugly solution: create a unique_ptr from its raw pointer and release the unique_ptr afterwards)
	auto nonnull = std::make_unique<BinaryExpression>(BinaryExpression::Operator::NEQ, cola::copy(expr), std::make_unique<NullValue>());
	return plankton::implies(formula, *nonnull);
}

bool plankton::implies(const Formula& /*formula*/, const Expression& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::implies(const Formula& /*formula*/, const Formula& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::implies(const Formula& /*formula*/, const std::vector<std::reference_wrapper<const Formula>>& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::is_equal(const Annotation& /*annotation*/, const Annotation& /*other*/) {
	throw std::logic_error("not yet implemented (plankton::is_equal)");
}

std::unique_ptr<Annotation> plankton::unify(const Annotation& /*annotation*/, const Annotation& /*other*/) {
	// creates an annotation that implies the passed annotations
	throw std::logic_error("not yet implemented (plankton::unify)");
}

std::unique_ptr<Annotation> plankton::unify(const std::vector<std::unique_ptr<Annotation>>& /*annotations*/) {
	// creates an annotation that implies the passed annotations
	throw std::logic_error("not yet implemented (plankton::unify)");
}
