#include "heal/logic.hpp"

#include <cassert>

using namespace cola;
using namespace heal;


class SymbolicVariablePoolImpl {
    static constexpr std::string_view PREFIX = "@";
    static std::deque<std::unique_ptr<SymbolicVariableDeclaration>> variables;

    static inline std::string MakeName(const Type& type) {
        switch (type.sort) {
            case Sort::VOID: return "v";
            case Sort::BOOL: return "b";
            case Sort::DATA: return "d";
            case Sort::PTR:  return "a";
        }
    }

    static inline std::string MakeSuffix() {
        return std::to_string(variables.size());
    }

    public:
    [[nodiscard]] static const SymbolicVariableDeclaration& MakeFresh(const Type& type) {
        std::string newName = std::string(PREFIX) + MakeName(type) + MakeSuffix();
        auto newVariable = std::make_unique<SymbolicVariableDeclaration>(newName, type, false);
        variables.push_back(std::move(newVariable));
        return *variables.back();
    }

    [[nodiscard]] static const decltype(variables)& GetAll() {
        return variables;
    }
};

const SymbolicVariableDeclaration& SymbolicVariablePool::MakeFresh(const cola::Type& type) {
    return SymbolicVariablePoolImpl::MakeFresh(type);
}

const std::deque<std::unique_ptr<SymbolicVariableDeclaration>>& SymbolicVariablePool::GetAll() {
    return SymbolicVariablePoolImpl::GetAll();
}


LogicVariable::LogicVariable(const VariableDeclaration& decl) : decl_storage(decl)  {
}

FlatSeparatingConjunction::FlatSeparatingConjunction() = default;

FlatSeparatingConjunction::FlatSeparatingConjunction(std::deque<std::unique_ptr<Axiom>> conjuncts_) : conjuncts(std::move(conjuncts_)) {
    for ([[maybe_unused]] const auto& conjunct : conjuncts) assert(conjunct);
}

SeparatingConjunction::SeparatingConjunction() = default;

SeparatingConjunction::SeparatingConjunction(std::deque<std::unique_ptr<Formula>> conjuncts_) : conjuncts(std::move(conjuncts_)) {
    for ([[maybe_unused]] const auto& conjunct : conjuncts) assert(conjunct);
}

SeparatingImplication::SeparatingImplication(std::unique_ptr<FlatFormula> premise_, std::unique_ptr<FlatFormula> conclusion_) : premise(std::move(premise_)), conclusion(std::move(conclusion_)) {
    assert(premise);
    assert(conclusion);
}

NegatedAxiom::NegatedAxiom(std::unique_ptr<Axiom> axiom_) : axiom(std::move(axiom_)) {
    assert(axiom);
}

PointsToAxiom::PointsToAxiom(std::unique_ptr<LogicVariable> node_, std::string fieldname_, std::unique_ptr<LogicVariable> value_) : node(std::move(node_)), fieldname(std::move(fieldname_)), value(std::move(value_)) {
    assert(node);
    assert(value);
}

StackAxiom::StackAxiom(std::unique_ptr<StackExpression> lhs_, Operator op_, std::unique_ptr<StackExpression> rhs_) : lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {
    assert(lhs);
    assert(rhs);
}

StackDisjunction::StackDisjunction() = default;

StackDisjunction::StackDisjunction(std::deque<std::unique_ptr<StackAxiom>> axioms_) : axioms(std::move(axioms_)) {
    for ([[maybe_unused]] const auto& axiom : axioms) assert(axiom);
}

OwnershipAxiom::OwnershipAxiom(std::unique_ptr<LogicVariable> node_) : node(std::move(node_)) {
	assert(node);
	assert(node->Decl().type.sort == Sort::PTR);
}

DataStructureLogicallyContainsAxiom::DataStructureLogicallyContainsAxiom(std::unique_ptr<LogicVariable> value_) : value(std::move(value_)) {
	assert(value);
	assert(value->Sort() == Sort::DATA);
}

NodeLogicallyContainsAxiom::NodeLogicallyContainsAxiom(std::unique_ptr<LogicVariable> node_, std::unique_ptr<LogicVariable> value_)
 : node(std::move(node_)), value(std::move(value_)) {
	assert(node);
	assert(node->Sort() == Sort::PTR);
	assert(value);
	assert(value->Sort() == Sort::DATA);
}

KeysetContainsAxiom::KeysetContainsAxiom(std::unique_ptr<LogicVariable> node_, std::unique_ptr<LogicVariable> value_)
 : node(std::move(node_)), value(std::move(value_)) {
	assert(node);
	assert(node->Sort() == Sort::PTR);
	assert(value);
	assert(value->Sort() == Sort::DATA);
}

HasFlowAxiom::HasFlowAxiom(std::unique_ptr<LogicVariable> node_) : node(std::move(node_)) {
	assert(node);
	assert(node->Sort() == Sort::PTR);
}

FlowContainsAxiom::FlowContainsAxiom(std::unique_ptr<LogicVariable> node_, std::unique_ptr<StackExpression> low_,std::unique_ptr<StackExpression> high_)
	: node(std::move(node_)), value_low(std::move(low_)), value_high(std::move(high_))
{
	assert(node);
	assert(node->Sort() == Sort::PTR);
	assert(value_low);
	assert(value_low->Sort() == Sort::DATA);
	assert(value_high);
	assert(value_high->Sort() == Sort::DATA);
}

UniqueInflowAxiom::UniqueInflowAxiom(std::unique_ptr<LogicVariable> node_) : node(std::move(node_)) {
	assert(node);
	assert(node->Sort() == Sort::PTR);
}

SpecificationAxiom::SpecificationAxiom(Kind kind_, std::unique_ptr<LogicVariable> key_) : kind(kind_), key(std::move(key_)) {
	assert(key);
	assert(key->Sort() == Sort::DATA);
}

ObligationAxiom::ObligationAxiom(Kind kind_, std::unique_ptr<LogicVariable> key_) : SpecificationAxiom(kind_, std::move(key_)) {
}

FulfillmentAxiom::FulfillmentAxiom(Kind kind_, std::unique_ptr<LogicVariable> key_, bool return_value_)
 : SpecificationAxiom(kind_, std::move(key_)), return_value(return_value_) {
}

PastPredicate::PastPredicate(std::unique_ptr<Formula> formula_) : formula(std::move(formula_)) {
	assert(formula);
}

FuturePredicate::FuturePredicate(std::unique_ptr<Formula> pre_, std::unique_ptr<Assignment> cmd_, std::unique_ptr<Formula> post_)
 : pre(std::move(pre_)), command(std::move(cmd_)), post(std::move(post_)) {
	assert(pre);
	assert(command);
	assert(post);
}

Annotation::Annotation() : now(std::make_unique<SeparatingConjunction>()) {
}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> formula_) : now(std::move(formula_)) {
    assert(now);
}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> formula_, std::deque<std::unique_ptr<TimePredicate>> time_) : now(std::move(formula_)), time(std::move(time_)) {
    assert(now);
    for ([[maybe_unused]] const auto& predicate : time) assert(predicate);
}

inline std::unique_ptr<Annotation> mk_bool(bool value) {
    auto sepcon = std::make_unique<SeparatingConjunction>();
    sepcon->conjuncts.push_back(std::make_unique<BoolAxiom>(value));
	return std::make_unique<Annotation>(std::move(sepcon));
}

std::unique_ptr<Annotation> Annotation::True() {
	return mk_bool(true);
}

std::unique_ptr<Annotation> Annotation::False() {
	return mk_bool(false);
}
