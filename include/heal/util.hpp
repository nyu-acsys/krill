#pragma once
#ifndef HEAL_UTIL
#define HEAL_UTIL


#include <memory>
#include <set>
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

	std::string ToString(const LogicObject& object);
    std::string ToString(SpecificationAxiom::Kind kind);
    std::string ToString(StackAxiom::Operator op);

    std::unique_ptr<Formula> Copy(const Formula& formula);
    std::unique_ptr<SeparatingConjunction> Copy(const SeparatingConjunction& formula);
    std::unique_ptr<FlatSeparatingConjunction> Copy(const FlatSeparatingConjunction& formula);
    std::unique_ptr<Axiom> Copy(const Axiom& formula);
    std::unique_ptr<StackAxiom> Copy(const StackAxiom& formula);
    std::unique_ptr<StackExpression> Copy(const StackExpression& expression);
    std::unique_ptr<LogicVariable> Copy(const LogicVariable& expression);
    std::unique_ptr<TimePredicate> Copy(const TimePredicate& predicate);
    std::unique_ptr<Annotation> Copy(const Annotation& annotation);

    std::unique_ptr<SeparatingConjunction> Conjoin(std::unique_ptr<Formula> formula, std::unique_ptr<Formula> other);
    std::unique_ptr<FlatSeparatingConjunction> Conjoin(std::unique_ptr<FlatFormula> formula, std::unique_ptr<FlatFormula> other);
    std::unique_ptr<Annotation> Conjoin(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other);

    void Simplify(LogicObject& object);

	//
	// Inspection
	//

    bool SyntacticallyEqual(const cola::Expression& object, const cola::Expression& other);
    bool SyntacticallyEqual(const LogicObject& object, const LogicObject& other);
    bool SyntacticallyContains(const LogicObject& object, const LogicObject& search, bool* insideObligation= nullptr);

	template<typename T, typename U>
	std::pair<bool, const T*> IsOfType(const U& object) {
		auto result = dynamic_cast<const T*>(&object);
		if (result) return std::make_pair(true, result);
		else return std::make_pair(false, nullptr);
	}

    struct SymbolicVariableSetComparator {
        bool operator() (const std::reference_wrapper<const SymbolicVariableDeclaration>& var, const std::reference_wrapper<const SymbolicVariableDeclaration>& other) const;
    };
    using SymbolicVariableSet = std::set<std::reference_wrapper<const SymbolicVariableDeclaration>, SymbolicVariableSetComparator>;

    bool IsSymbolicVariable(const cola::VariableDeclaration& decl);
    bool IsSymbolicVariable(const LogicVariable& variable);
    const SymbolicVariableDeclaration& MakeFreshSymbolicVariable(const cola::Type& type);
    const SymbolicVariableDeclaration& GetUnusedSymbolicVariable(const cola::Type& type, const SymbolicVariableSet& variablesInUse);
    const SymbolicVariableDeclaration& GetUnusedSymbolicVariable(const cola::Type& type, const LogicObject& unusedInObject);
    SymbolicVariableSet CollectSymbolicVariables(const LogicObject& object);

    //
    // Transformation
    //

    using transformer_t = std::function<const cola::VariableDeclaration*(const cola::VariableDeclaration&)>;

    std::unique_ptr<LogicObject> Replace(std::unique_ptr<LogicObject> object, const transformer_t& transformer, bool* changed= nullptr);
    std::unique_ptr<SeparatingConjunction> Replace(std::unique_ptr<SeparatingConjunction> formula, const transformer_t& transformer, bool* changed= nullptr);
    std::unique_ptr<TimePredicate> Replace(std::unique_ptr<TimePredicate> predicate, const transformer_t& transformer, bool* changed= nullptr);
    std::unique_ptr<Annotation> Replace(std::unique_ptr<Annotation> annotation, const transformer_t& transformer, bool* changed= nullptr);

    std::unique_ptr<LogicObject> RenameSymbolicVariables(std::unique_ptr<LogicObject> object, const LogicObject& avoidSymbolicVariablesFrom, bool* changed= nullptr);
    std::unique_ptr<SeparatingConjunction> RenameSymbolicVariables(std::unique_ptr<SeparatingConjunction> formula, const LogicObject& avoidSymbolicVariablesFrom, bool* changed= nullptr);
    std::unique_ptr<TimePredicate> RenameSymbolicVariables(std::unique_ptr<TimePredicate> predicate, const LogicObject& avoidSymbolicVariablesFrom, bool* changed= nullptr);
    std::unique_ptr<Annotation> RenameSymbolicVariables(std::unique_ptr<Annotation> annotation, const LogicObject& avoidSymbolicVariablesFrom, bool* changed= nullptr);

    //
    // Construction
    //

	std::unique_ptr<SymbolicNull> MakeNull();
	std::unique_ptr<SymbolicBool> MakeBool(bool val);
	std::unique_ptr<SymbolicBool> MakeTrue();
	std::unique_ptr<SymbolicBool> MakeFalse();
	std::unique_ptr<SymbolicMax> MakeMax();
	std::unique_ptr<SymbolicMin> MakeMin();
    std::unique_ptr<LogicVariable> MakeVar(const cola::VariableDeclaration& decl);

    std::unique_ptr<NegatedAxiom> MakeNegation(std::unique_ptr<Axiom> axiom);
    std::unique_ptr<BoolAxiom> MakeBoolAxiom(bool value);
    std::unique_ptr<PointsToAxiom> MakePointsToAxiom(const cola::VariableDeclaration& node, std::string field, const cola::VariableDeclaration& value);
    std::unique_ptr<StackAxiom> MakeStackAxiom(std::unique_ptr<StackExpression> lhs, StackAxiom::Operator op, std::unique_ptr<StackExpression> rhs);
	std::unique_ptr<OwnershipAxiom> MakeOwnershipAxiom(const cola::VariableDeclaration& decl);
	std::unique_ptr<ObligationAxiom> MakeObligationAxiom(SpecificationAxiom::Kind kind, const cola::VariableDeclaration& decl);
	std::unique_ptr<FulfillmentAxiom> MakeFulfillmentAxiom(SpecificationAxiom::Kind kind, const cola::VariableDeclaration& decl, bool returnValue);
	std::unique_ptr<DataStructureLogicallyContainsAxiom> MakeDatastructureContainsAxiom(const cola::VariableDeclaration& value);
	std::unique_ptr<NodeLogicallyContainsAxiom> MakeNodeContainsAxiom(const cola::VariableDeclaration& node, const cola::VariableDeclaration& value);
	std::unique_ptr<HasFlowAxiom> MakeHasFlowAxiom(const cola::VariableDeclaration& expr);
	std::unique_ptr<KeysetContainsAxiom> MakeKeysetContainsAxiom(const cola::VariableDeclaration& expr, const cola::VariableDeclaration& other);
	std::unique_ptr<FlowContainsAxiom> MakeFlowContainsAxiom(const cola::VariableDeclaration& expr, std::unique_ptr<StackExpression> low, std::unique_ptr<StackExpression> high);
	std::unique_ptr<UniqueInflowAxiom> MakeUniqueInflowAxiom(const cola::VariableDeclaration& expr);

	std::unique_ptr<StackDisjunction> MakeDisjunction(std::deque<std::unique_ptr<StackAxiom>> disjuncts);
    std::unique_ptr<SeparatingConjunction> MakeConjunction(std::deque<std::unique_ptr<Formula>> conjuncts);
    std::unique_ptr<FlatSeparatingConjunction> MakeFlatConjunction(std::deque<std::unique_ptr<FlatFormula>> conjuncts);
	std::unique_ptr<SeparatingImplication> MakeImplication();
	std::unique_ptr<SeparatingImplication> MakeImplication(std::unique_ptr<FlatSeparatingConjunction> premise, std::unique_ptr<FlatSeparatingConjunction> conclusion);

    template<typename R, typename... T>
    std::deque<std::unique_ptr<R>> MakeDeque(T... elems) {
        std::array<std::unique_ptr<R> , sizeof...(elems)> elemArray = { std::forward<T>(elems)... };
        auto beginIterator = std::make_move_iterator(elemArray.begin());
        auto endIterator = std::make_move_iterator(elemArray.end());
        return std::deque<std::unique_ptr<R>>(beginIterator, endIterator);
    }

    template<typename... T>
    std::unique_ptr<SeparatingConjunction> MakeConjunction(T... conjuncts) {
        return MakeConjunction(MakeDeque<Formula>(std::forward<T>(conjuncts)...));
    }

    template<typename... T>
    std::unique_ptr<FlatSeparatingConjunction> MakeFlatConjunction(T... conjuncts) {
        return MakeFlatConjunction(MakeDeque<FlatFormula>(std::forward<T>(conjuncts)...));
    }

    template<typename... T>
    std::unique_ptr<StackDisjunction> MakeDisjunction(T... disjuncts) {
        return MakeDisjunction(MakeDeque<StackAxiom>(std::forward<T>(disjuncts)...));
    }


} // namespace heal

#endif