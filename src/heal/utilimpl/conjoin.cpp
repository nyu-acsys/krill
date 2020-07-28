#include "heal/util.hpp"

using namespace cola;
using namespace heal;


static constexpr bool SIMPLIFY_CONJUNCTION = true; // TODO: have a configuration file for this

template<typename U, typename V>
std::unique_ptr<U> make_conjunction(std::unique_ptr<U> formula, std::unique_ptr<V> other) {
	if (!SIMPLIFY_CONJUNCTION) {
		// just copy 'other' over to 'formula'
		formula->conjuncts.insert(
			formula->conjuncts.end(),
			std::make_move_iterator(other->conjuncts.begin()),
			std::make_move_iterator(other->conjuncts.end())
		);
		return formula;
	}

	// only copy conjuncts from 'other' that are not contained in 'formula'
	for (auto& conjunct : *other) {
		if (!syntactically_contains_conjunct(*formula, *conjunct)) {
			formula->conjuncts.push_back(std::move(conjunct));
		}
	}
	return formula;
}

std::unique_ptr<ConjunctionFormula> heal::conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<ConjunctionFormula> other) {
	return make_conjunction(std::move(formula), std::move(other));
}

std::unique_ptr<ConjunctionFormula> heal::conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<AxiomConjunctionFormula> other) {
	return make_conjunction(std::move(formula), std::move(other));
}

std::unique_ptr<AxiomConjunctionFormula> heal::conjoin(std::unique_ptr<AxiomConjunctionFormula> formula, std::unique_ptr<AxiomConjunctionFormula> other) {
	return make_conjunction(std::move(formula), std::move(other));
}