#pragma once
#ifndef PLANKTON_UTIL
#define PLANKTON_UTIL


#include <memory>
#include <functional>
#include "cola/ast.hpp"
#include "plankton/logic.hpp"


namespace plankton {

	void print(const Formula& formula, std::ostream& stream);
	void print(const TimePredicate& TimePredicate, std::ostream& stream);
	void print(const Annotation& annotation, std::ostream& stream);


	std::unique_ptr<Formula> copy(const Formula& formula);
	std::unique_ptr<TimePredicate> copy(const TimePredicate& predicate);
	std::unique_ptr<Annotation> copy(const Annotation& annotation);


	std::unique_ptr<ConjunctionFormula> flatten(std::unique_ptr<Formula> formula);
	std::unique_ptr<ConjunctionFormula> flatten(const Formula& formula);


	bool syntactically_equal(const cola::Expression& expression, const cola::Expression& other);

	
	bool contains_expression(const Formula& formula, const cola::Expression& search);
	bool contains_expression(const TimePredicate& predicate, const cola::Expression& search);


	using transformer_t = std::function<std::pair<bool,std::unique_ptr<cola::Expression>>(const cola::Expression&)>;
	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> expression, transformer_t transformer);
	std::unique_ptr<Formula> replace_expression(std::unique_ptr<Formula> formula, transformer_t transformer);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> predicate, transformer_t transformer);

	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<Formula> replace_expression(std::unique_ptr<Formula> formula, const cola::Expression& replace, const cola::Expression& with);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> predicate, const cola::Expression& replace, const cola::Expression& with);

} // namespace plankton

#endif