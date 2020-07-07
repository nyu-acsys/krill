#pragma once
#ifndef PLANKTON_UTIL
#define PLANKTON_UTIL


#include <memory>
#include <vector>
#include <ostream>
#include <functional>
#include <type_traits>
#include "cola/ast.hpp"
#include "plankton/logic.hpp"


namespace plankton {
	
	#define EXTENDS_FORMULA(T) typename = std::enable_if_t<std::is_base_of_v<Formula, T>>

	//
	// Basics
	//
	
	void print(const Formula& formula, std::ostream& stream);

	std::unique_ptr<Formula> copy(const Formula& formula);
	std::unique_ptr<SimpleFormula> copy(const SimpleFormula& formula);
	std::unique_ptr<Axiom> copy(const Axiom& formula);
	std::unique_ptr<TimePredicate> copy(const TimePredicate& formula);
	std::unique_ptr<AxiomConjunctionFormula> copy(const AxiomConjunctionFormula& formula);
	std::unique_ptr<ImplicationFormula> copy(const ImplicationFormula& formula);
	std::unique_ptr<ConjunctionFormula> copy(const ConjunctionFormula& formula);
	std::unique_ptr<NegatedAxiom> copy(const NegatedAxiom& formula);
	std::unique_ptr<ExpressionAxiom> copy(const ExpressionAxiom& formula);
	std::unique_ptr<OwnershipAxiom> copy(const OwnershipAxiom& formula);
	std::unique_ptr<LogicallyContainedAxiom> copy(const LogicallyContainedAxiom& formula);
	std::unique_ptr<KeysetContainsAxiom> copy(const KeysetContainsAxiom& formula);
	std::unique_ptr<HasFlowAxiom> copy(const HasFlowAxiom& formula);
	std::unique_ptr<FlowContainsAxiom> copy(const FlowContainsAxiom& formula);
	std::unique_ptr<ObligationAxiom> copy(const ObligationAxiom& formula);
	std::unique_ptr<FulfillmentAxiom> copy(const FulfillmentAxiom& formula);
	std::unique_ptr<PastPredicate> copy(const PastPredicate& formula);
	std::unique_ptr<FuturePredicate> copy(const FuturePredicate& formula);
	std::unique_ptr<Annotation> copy(const Annotation& formula);

	std::unique_ptr<ConjunctionFormula> conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<ConjunctionFormula> other);
	std::unique_ptr<ConjunctionFormula> conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<AxiomConjunctionFormula> other);
	std::unique_ptr<AxiomConjunctionFormula> conjoin(std::unique_ptr<AxiomConjunctionFormula> formula, std::unique_ptr<AxiomConjunctionFormula> other);

	//
	// Inspection
	//

	bool syntactically_equal(const cola::Expression& expression, const cola::Expression& other);
	bool syntactically_equal(const Formula& formula, const Formula& other);
	bool syntactically_contains_conjunct(const ConjunctionFormula& formula, const SimpleFormula& other);
	bool syntactically_contains_conjunct(const AxiomConjunctionFormula& formula, const SimpleFormula& other);

	template<typename T, typename U>
	std::pair<bool, const T*> is_of_type(const U& formula) {
		auto result = dynamic_cast<const T*>(&formula);
		if (result) return std::make_pair(true, result);
		else return std::make_pair(false, nullptr);
	}
	
	/** Test whether 'formula' contains 'search' as subexpression somewhere.
	  */
	bool contains_expression(const Formula& formula, const cola::Expression& search);

	/** Test whether 'formula' contains 'search' as subexpression.
	  * Returns a pair 'p' where 'p.first' indicates whether 'search' was found somewhere
	  * and 'p.second' indicates that 'search' was found inside an 'ObligationAxiom'.
	  */
	std::pair<bool, bool> contains_expression_obligation(const Formula& formula, const cola::Expression& search);
	
	//
	// Transformation
	//

	using transformer_t = std::function<std::pair<bool,std::unique_ptr<cola::Expression>>(const cola::Expression&)>;

	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> expression, transformer_t transformer);
	std::unique_ptr<Formula> replace_expression(std::unique_ptr<Formula> formula, transformer_t transformer);
	std::unique_ptr<SimpleFormula> replace_expression(std::unique_ptr<SimpleFormula> formula, transformer_t transformer);
	std::unique_ptr<Axiom> replace_expression(std::unique_ptr<Axiom> formula, transformer_t transformer);
	std::unique_ptr<ConjunctionFormula> replace_expression(std::unique_ptr<ConjunctionFormula> formula, transformer_t transformer);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> formula, transformer_t transformer);

	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<Formula> replace_expression(std::unique_ptr<Formula> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<SimpleFormula> replace_expression(std::unique_ptr<SimpleFormula> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<Axiom> replace_expression(std::unique_ptr<Axiom> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<ConjunctionFormula> replace_expression(std::unique_ptr<ConjunctionFormula> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> formula, const cola::Expression& replace, const cola::Expression& with);

	std::unique_ptr<AxiomConjunctionFormula> flatten(std::unique_ptr<cola::Expression> expression);

} // namespace plankton

#endif