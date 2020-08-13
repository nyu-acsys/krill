#pragma once
#ifndef HEAL_UTIL
#define HEAL_UTIL


#include <memory>
#include <vector>
#include <ostream>
#include <functional>
#include <type_traits>
#include "cola/ast.hpp"
#include "heal/logic.hpp"


namespace heal {
	
	#define EXTENDS_FORMULA(T) typename = std::enable_if_t<std::is_base_of_v<Formula, T>>

	//
	// Basics
	//
	
	void print(const Formula& formula, std::ostream& stream);

	std::unique_ptr<Formula> copy(const Formula& formula);
	std::unique_ptr<NowFormula> copy(const NowFormula& formula);
	std::unique_ptr<SimpleFormula> copy(const SimpleFormula& formula);
	std::unique_ptr<Axiom> copy(const Axiom& formula);
	std::unique_ptr<TimePredicate> copy(const TimePredicate& formula);
	std::unique_ptr<AxiomConjunctionFormula> copy(const AxiomConjunctionFormula& formula);
	std::unique_ptr<ImplicationFormula> copy(const ImplicationFormula& formula);
	std::unique_ptr<ConjunctionFormula> copy(const ConjunctionFormula& formula);
	std::unique_ptr<NegatedAxiom> copy(const NegatedAxiom& formula);
	std::unique_ptr<ExpressionAxiom> copy(const ExpressionAxiom& formula);
	std::unique_ptr<OwnershipAxiom> copy(const OwnershipAxiom& formula);
	std::unique_ptr<DataStructureLogicallyContainsAxiom> copy(const DataStructureLogicallyContainsAxiom& formula);
	std::unique_ptr<NodeLogicallyContainsAxiom> copy(const NodeLogicallyContainsAxiom& formula);
	std::unique_ptr<KeysetContainsAxiom> copy(const KeysetContainsAxiom& formula);
	std::unique_ptr<HasFlowAxiom> copy(const HasFlowAxiom& formula);
	std::unique_ptr<FlowContainsAxiom> copy(const FlowContainsAxiom& formula);
	std::unique_ptr<UniqueInflowAxiom> copy(const UniqueInflowAxiom& formula);
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
	bool syntactically_contains_conjunct(const Formula& formula, const SimpleFormula& other);
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
	bool contains_expression(const cola::Expression& expression, const cola::Expression& search);

	/** Test whether 'formula' contains 'search' as subexpression.
	  * Returns a pair 'p' where 'p.first' indicates whether 'search' was found somewhere
	  * and 'p.second' indicates that 'search' was found inside an 'ObligationAxiom'.
	  */
	std::pair<bool, bool> contains_expression_obligation(const Formula& formula, const cola::Expression& search);
	
	//
	// Transformation
	//

	using transformer_t = std::function<std::pair<bool,std::unique_ptr<cola::Expression>>(const cola::Expression&)>;
	template<typename T> using unary_t = std::function<bool(const T&)>;

	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> expression, const transformer_t& transformer, bool* changed=nullptr);
	std::unique_ptr<Formula> replace_expression(std::unique_ptr<Formula> formula, const transformer_t& transformer, bool* changed=nullptr);
	std::unique_ptr<SimpleFormula> replace_expression(std::unique_ptr<SimpleFormula> formula, const transformer_t& transformer, bool* changed=nullptr);
	std::unique_ptr<Axiom> replace_expression(std::unique_ptr<Axiom> formula, const transformer_t& transformer, bool* changed=nullptr);
	std::unique_ptr<ConjunctionFormula> replace_expression(std::unique_ptr<ConjunctionFormula> formula, const transformer_t& transformer, bool* changed=nullptr);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> formula, const transformer_t& transformer, bool* changed=nullptr);

	std::unique_ptr<cola::Expression> replace_expression(std::unique_ptr<cola::Expression> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);
	std::unique_ptr<Formula> replace_expression(std::unique_ptr<Formula> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);
	std::unique_ptr<SimpleFormula> replace_expression(std::unique_ptr<SimpleFormula> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);
	std::unique_ptr<Axiom> replace_expression(std::unique_ptr<Axiom> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);
	std::unique_ptr<ConjunctionFormula> replace_expression(std::unique_ptr<ConjunctionFormula> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);
	std::unique_ptr<TimePredicate> replace_expression(std::unique_ptr<TimePredicate> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);

	std::unique_ptr<AxiomConjunctionFormula> flatten(std::unique_ptr<cola::Expression> expression);

	std::unique_ptr<ConjunctionFormula> remove_conjuncts_if(std::unique_ptr<ConjunctionFormula> formula, const unary_t<SimpleFormula>& unaryPredicate, bool* changed=nullptr);
	
	//
	// Construction
	//

	std::unique_ptr<cola::NullValue> MakeNullExpr();
	std::unique_ptr<cola::BooleanValue> MakeBoolExpr(bool val);
	std::unique_ptr<cola::BooleanValue> MakeTrueExpr();
	std::unique_ptr<cola::BooleanValue> MakeFalseExpr();
	std::unique_ptr<cola::MaxValue> MakeMaxExpr();
	std::unique_ptr<cola::MinValue> MakeMinExpr();

	std::unique_ptr<cola::VariableExpression> MakeExpr(const cola::VariableDeclaration& decl);
	std::unique_ptr<cola::BinaryExpression> MakeExpr(cola::BinaryExpression::Operator op, std::unique_ptr<cola::Expression> lhs, std::unique_ptr<cola::Expression> rhs);
	std::unique_ptr<cola::Dereference> MakeDerefExpr(const cola::VariableDeclaration& decl, std::string field);

	std::unique_ptr<ExpressionAxiom> MakeAxiom(std::unique_ptr<cola::Expression> expr);
	std::unique_ptr<OwnershipAxiom> MakeOwnershipAxiom(const cola::VariableDeclaration& decl);
	std::unique_ptr<NegatedAxiom> MakeNegatedAxiom(std::unique_ptr<Axiom> axiom);
	std::unique_ptr<ObligationAxiom> MakeObligationAxiom(SpecificationAxiom::Kind kind, const cola::VariableDeclaration& decl);
	std::unique_ptr<FulfillmentAxiom> MakeFulfillmentAxiom(SpecificationAxiom::Kind kind, const cola::VariableDeclaration& decl, bool returnValue);
	std::unique_ptr<DataStructureLogicallyContainsAxiom> MakeDatastructureContainsAxiom(std::unique_ptr<cola::Expression> value);
	std::unique_ptr<NodeLogicallyContainsAxiom> MakeNodeContainsAxiom(std::unique_ptr<cola::Expression> node, std::unique_ptr<cola::Expression> value);
	std::unique_ptr<HasFlowAxiom> MakeHasFlowAxiom(std::unique_ptr<cola::Expression> expr);
	std::unique_ptr<KeysetContainsAxiom> MakeKeysetContainsAxiom(std::unique_ptr<cola::Expression> expr, std::unique_ptr<cola::Expression> other);
	std::unique_ptr<FlowContainsAxiom> MakeFlowContainsAxiom(std::unique_ptr<cola::Expression> expr, std::unique_ptr<cola::Expression> low, std::unique_ptr<cola::Expression> high);
	std::unique_ptr<UniqueInflowAxiom> MakeUniqueInflowAxiom(std::unique_ptr<cola::Expression> expr, std::unique_ptr<cola::Expression> low, std::unique_ptr<cola::Expression> high);

	std::unique_ptr<ConjunctionFormula> MakeConjunction();
	std::unique_ptr<ConjunctionFormula> MakeConjunction(std::deque<std::unique_ptr<SimpleFormula>> conjuncts);
	std::unique_ptr<AxiomConjunctionFormula> MakeAxiomConjunction();
	std::unique_ptr<AxiomConjunctionFormula> MakeAxiomConjunction(std::deque<std::unique_ptr<Axiom>> conjuncts);
	std::unique_ptr<ImplicationFormula> MakeImplication();
	std::unique_ptr<ImplicationFormula> MakeImplication(std::unique_ptr<Axiom> premise, std::unique_ptr<Axiom> conclusion);
	std::unique_ptr<ImplicationFormula> MakeImplication(std::unique_ptr<AxiomConjunctionFormula> premise, std::unique_ptr<AxiomConjunctionFormula> conclusion);


	template<typename R, typename... T>
	std::deque<std::unique_ptr<R>> MakeDeque(T... elems) {
		std::array<std::unique_ptr<R> , sizeof...(elems)> elemArray = { std::forward<T>(elems)... };
		auto beginIterator = std::make_move_iterator(elemArray.begin());
		auto endIterator = std::make_move_iterator(elemArray.end());
		return std::deque<std::unique_ptr<R>>(beginIterator, endIterator);
	}
	template<typename... T>
	std::unique_ptr<ConjunctionFormula> MakeConjunction(T... conjuncts) {
		return MakeConjunction(MakeDeque<SimpleFormula>(std::forward<T>(conjuncts)...));
	}
	template<typename... T>
	std::unique_ptr<AxiomConjunctionFormula> MakeAxiomConjunction(T... conjuncts) {
		return MakeAxiomConjunction(MakeDeque<Axiom>(std::forward<T>(conjuncts)...));
	}

} // namespace heal

#endif