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
	std::unique_ptr<FlowAxiom> copy(const FlowAxiom& formula);
	std::unique_ptr<ObligationAxiom> copy(const ObligationAxiom& formula);
	std::unique_ptr<FulfillmentAxiom> copy(const FulfillmentAxiom& formula);
	std::unique_ptr<PastPredicate> copy(const PastPredicate& formula);
	std::unique_ptr<FuturePredicate> copy(const FuturePredicate& formula);
	std::unique_ptr<Annotation> copy(const Annotation& formula);

	std::unique_ptr<ConjunctionFormula> conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<ConjunctionFormula> other);
	std::unique_ptr<ConjunctionFormula> conjoin(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<AxiomConjunctionFormula> other);

	//
	// Inspection
	//

	bool syntactically_equal(const cola::Expression& expression, const cola::Expression& other);
	bool syntactically_equal(const Formula& formula, const Formula& other);
	bool syntactically_contains_conjunct(const ConjunctionFormula& formula, const SimpleFormula& other);

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

	std::unique_ptr<Axiom> instantiate_property(const cola::Property& property, std::vector<std::reference_wrapper<const cola::VariableDeclaration>> vars);
	std::unique_ptr<AxiomConjunctionFormula> instantiate_property_flatten(const cola::Property& property, std::vector<std::reference_wrapper<const cola::VariableDeclaration>> vars);

	std::unique_ptr<AxiomConjunctionFormula> flatten(std::unique_ptr<cola::Expression> expression);

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

	/** Returns true if 'annotation ==> other' and 'other ==> annotation'.
	  */
	bool is_equal(const Annotation& /*annotation*/, const Annotation& /*other*/);

	/** Returns an annotation F such that the formulas 'annotation ==> F' and 'other ==> F' are tautologies.
	  * Opts for the strongest such annotation F.
	  */
	std::unique_ptr<Annotation> unify(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other);

	/** Returns an annotation F such that the formula 'H ==> F' is a tautology for all passed annotations H.
	  * Opts for the strongest such annotation F.
	  */
	std::unique_ptr<Annotation> unify(std::vector<std::unique_ptr<Annotation>> annotations);

} // namespace plankton

#endif