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

	template<typename F, EXTENDS_FORMULA(F)>
	std::unique_ptr<F> copy(const F& formula);

	//
	// Inspection
	//

	bool syntactically_equal(const cola::Expression& expression, const cola::Expression& other);

	template<typename F, EXTENDS_FORMULA(F)>
	std::pair<bool, const F*> is_of_type(const Formula& formula);
	
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
	template<typename F, EXTENDS_FORMULA(F)>
	std::unique_ptr<F> replace_expression(std::unique_ptr<F> formula, transformer_t transformer);

	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> formula, const cola::Expression& replace, const cola::Expression& with);
	template<typename F, EXTENDS_FORMULA(F)>
	std::unique_ptr<F> replace_expression(std::unique_ptr<F> formula, const cola::Expression& replace, const cola::Expression& with);

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