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
	//typename = std::enable_if<std::is_base_of<Formula, T>::value>

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
	std::unique_ptr<FlowAxiom> copy(const FlowAxiom& formula);
	std::unique_ptr<ObligationAxiom> copy(const ObligationAxiom& formula);
	std::unique_ptr<FulfillmentAxiom> copy(const FulfillmentAxiom& formula);
	std::unique_ptr<PastPredicate> copy(const PastPredicate& formula);
	std::unique_ptr<FuturePredicate> copy(const FuturePredicate& formula);
	std::unique_ptr<Annotation> copy(const Annotation& formula);

	//
	// Inspection
	//

	bool syntactically_equal(const cola::Expression& expression, const cola::Expression& other);

	template<typename F, EXTENDS_FORMULA(F)>
	std::pair<bool, const F*> is_of_type(const Formula& formula) {
		auto result = dynamic_cast<const F*>(&formula);
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
	std::unique_ptr<ConjunctionFormula> replace_expression(std::unique_ptr<ConjunctionFormula> formula, transformer_t transformer);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> formula, transformer_t transformer);

	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<Formula> replace_expression(std::unique_ptr<Formula> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<SimpleFormula> replace_expression(std::unique_ptr<SimpleFormula> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<ConjunctionFormula> replace_expression(std::unique_ptr<ConjunctionFormula> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> formula, const cola::Expression& replace, const cola::Expression& with);

	//
	// Solving
	//

	/** Returns true if 'formula ==> (expr != NULL)' is a tautology.
	  */
	bool implies_nonnull(const Formula& formula, const cola::Expression& expr);
	
	/** Returns true if 'formula ==> condition' is a tautology.
	  */
	bool implies(const Formula& formula, const cola::Expression& condition);

	/** Returns true if 'formula ==> implied' is a tautology.
	  */
	bool implies(const Formula& formula, const Formula& implied);

	/** Returns true if 'formula ==> implied[i]' is a tautology for all '0 <= i < implied.size()'.
	  */
	bool implies(const Formula& formula, const std::vector<std::reference_wrapper<const Formula>>& implied);

	/** Returns true if 'annotation ==> other' and 'other ==> annotation'.
	  */
	bool is_equal(const Annotation& /*annotation*/, const Annotation& /*other*/);

	/** Returns a formula F such that the formulas 'formula ==> F' and 'other ==> F' are tautologies.
	  * Opts for the strongest such formula F.
	  */
	std::unique_ptr<Annotation> unify(const Annotation& annotation, const Annotation& other);

	/** Returns a formula F such that the formula 'H ==> F' is a tautology for all H in formulas.
	  * Opts for the strongest such formula F.
	  */
	std::unique_ptr<Annotation> unify(const std::vector<std::unique_ptr<Annotation>>& annotations);

} // namespace plankton

#endif