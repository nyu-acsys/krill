#include "plankton/util.hpp"

#include "plankton/config.hpp"

using namespace cola;
using namespace plankton;


template<typename U, typename V>
std::unique_ptr<ConjunctionFormula> make_conjunction(std::unique_ptr<U> formula, std::unique_ptr<V> other) {
	if (!plankton::config->conjoin_simplify) {
		// just copy 'other' over to 'formula'
		formula->conjuncts.insert(
			formula->conjuncts.end(),
			std::make_move_iterator(other->conjuncts.begin()),
			std::make_move_iterator(other->conjuncts.end())
		);
		return formula;
	}

	// only copy conjuncts from 'other' that are not contained in 'formula'
	for (auto& conjunct : other->conjuncts) {
		if (!syntactically_contains_conjunct(*formula, *conjunct)) {
			formula->conjuncts.push_back(std::move(conjunct));
		}
	}
	return formula;
}

std::unique_ptr<ConjunctionFormula> plankton::conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<ConjunctionFormula> other) {
	return make_conjunction(std::move(formula), std::move(other));
}

std::unique_ptr<ConjunctionFormula> plankton::conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<AxiomConjunctionFormula> other) {
	return make_conjunction(std::move(formula), std::move(other));
}