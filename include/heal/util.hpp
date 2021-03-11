#pragma once
#ifndef HEAL_UTIL
#define HEAL_UTIL


#include <memory>
#include <vector>
#include <ostream>
#include <functional>
#include <type_traits>
#include "cola/ast.hpp"
#include "logic.hpp"


namespace heal {

	// #define EXTENDS_FORMULA(T) typename = std::enable_if_t<std::is_base_of_v<Formula, T>> /* TODO: use macro instead of inlined typename */

	//
	// Basics
	//

	void Print(const LogicObject& object, std::ostream& stream);

    std::unique_ptr<Formula> Copy(const Formula& formula);
    std::unique_ptr<TimePredicate> Copy(const TimePredicate& predicate);
    std::unique_ptr<Annotation> Copy(const Annotation& annotation);

    std::unique_ptr<Formula> Conjoin(std::unique_ptr<Formula> formula, std::unique_ptr<Formula> other);
    std::unique_ptr<Annotation> Conjoin(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other);

	//
	// Inspection
	//

	bool SyntacticallyEqual(const cola::Expression& expression, const cola::Expression& other);
    bool SyntacticallyEqual(const LogicObject& object, const LogicObject& other);

    bool ContainsExpression(const cola::Expression& expression, const cola::Expression& search);
    bool ContainsExpression(const LogicObject& object, const cola::Expression& search, bool* insideObligation= nullptr);

	template<typename T, typename U>
	std::pair<bool, const T*> IsOfType(const U& object) {
		auto result = dynamic_cast<const T*>(&object);
		if (result) return std::make_pair(true, result);
		else return std::make_pair(false, nullptr);
	}
	
	//
	// Transformation
	//

	using transformer_t = std::function<std::pair<bool,std::unique_ptr<cola::Expression>>(const cola::Expression&)>;

	std::unique_ptr<cola::Expression> ReplaceExpression(std::unique_ptr<cola::Expression> expression, const transformer_t& transformer, bool* changed= nullptr);
    std::unique_ptr<Formula> ReplaceExpression(std::unique_ptr<Formula> formula, const transformer_t& transformer, bool* changed=nullptr);
    std::unique_ptr<TimePredicate> ReplaceExpression(std::unique_ptr<TimePredicate> uniquePtr, const transformer_t& transformer, bool* changed=nullptr);
    std::unique_ptr<Annotation> ReplaceExpression(std::unique_ptr<Annotation> annotation, const transformer_t& transformer, bool* changed=nullptr);

	std::unique_ptr<cola::Expression> ReplaceExpression(std::unique_ptr<cola::Expression> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed= nullptr);
    std::unique_ptr<Formula> ReplaceExpression(std::unique_ptr<Formula> formula, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);
    std::unique_ptr<TimePredicate> ReplaceExpression(std::unique_ptr<TimePredicate> predicate, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);
    std::unique_ptr<Annotation> ReplaceExpression(std::unique_ptr<Annotation> annotation, const cola::Expression& replace, const cola::Expression& with, bool* changed=nullptr);

    std::unique_ptr<cola::Expression> Simplify(std::unique_ptr<cola::Expression> expression);
    std::unique_ptr<Formula> Simplify(std::unique_ptr<Formula> formula);
    std::unique_ptr<TimePredicate> Simplify(std::unique_ptr<TimePredicate> predicate);
    std::unique_ptr<Annotation> Simplify(std::unique_ptr<Annotation> annotation);
	
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
	std::unique_ptr<ObligationAxiom> MakeObligationAxiom(SpecificationAxiom::Kind kind, const cola::VariableDeclaration& decl);
	std::unique_ptr<FulfillmentAxiom> MakeFulfillmentAxiom(SpecificationAxiom::Kind kind, const cola::VariableDeclaration& decl, bool returnValue);
	std::unique_ptr<DataStructureLogicallyContainsAxiom> MakeDatastructureContainsAxiom(std::unique_ptr<cola::Expression> value);
	std::unique_ptr<NodeLogicallyContainsAxiom> MakeNodeContainsAxiom(std::unique_ptr<cola::Expression> node, std::unique_ptr<cola::Expression> value);
	std::unique_ptr<HasFlowAxiom> MakeHasFlowAxiom(std::unique_ptr<cola::Expression> expr);
	std::unique_ptr<KeysetContainsAxiom> MakeKeysetContainsAxiom(std::unique_ptr<cola::Expression> expr, std::unique_ptr<cola::Expression> other);
	std::unique_ptr<FlowContainsAxiom> MakeFlowContainsAxiom(std::unique_ptr<cola::Expression> expr, std::unique_ptr<cola::Expression> low, std::unique_ptr<cola::Expression> high);
	std::unique_ptr<UniqueInflowAxiom> MakeUniqueInflowAxiom(std::unique_ptr<cola::Expression> expr);

	std::unique_ptr<ConjunctionFormula> MakeConjunction();
	std::unique_ptr<ConjunctionFormula> MakeConjunction(std::deque<std::unique_ptr<Formula>> conjuncts);
	std::unique_ptr<ImplicationFormula> MakeImplication();
	std::unique_ptr<ImplicationFormula> MakeImplication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion);
    std::unique_ptr<NegationFormula> MakeNegation(std::unique_ptr<Formula> formula);

	template<typename... T>
	std::unique_ptr<ConjunctionFormula> MakeConjunction(T... conjuncts) {
        std::array<std::unique_ptr<Formula> , sizeof...(conjuncts)> elemArray = { std::forward<T>(conjuncts)... };
        auto beginIterator = std::make_move_iterator(elemArray.begin());
        auto endIterator = std::make_move_iterator(elemArray.end());
        return MakeConjunction(std::deque<std::unique_ptr<Formula>>(beginIterator, endIterator));
	}

} // namespace heal

#endif