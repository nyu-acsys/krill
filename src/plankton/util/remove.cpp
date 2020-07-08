#include "plankton/util.hpp"

using namespace plankton;

std::unique_ptr<ConjunctionFormula> plankton::remove_conjuncts_if(std::unique_ptr<ConjunctionFormula> formula, unary_t<SimpleFormula> unaryPredicate) {
	auto& container = formula->conjuncts;
	container.erase(
		std::remove_if(std::begin(container), std::end(container), [&unaryPredicate](const auto& uptr) { return unaryPredicate(*uptr); }),
		std::end(container)
	);
	return formula;
}