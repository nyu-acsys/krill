#include "heal/logic.hpp"

#include <cassert>

using namespace cola;
using namespace heal;


class SymbolicVariablePoolImpl {
    static constexpr std::string_view PREFIX = "@";
    static std::deque<std::unique_ptr<SymbolicVariableDeclaration>> variables;
    static std::deque<std::unique_ptr<SymbolicFlowDeclaration>> flows;

    static inline std::string MakeName(const Type &type, bool firstOrder) {
        switch (type.sort) {
            case Sort::VOID: return firstOrder ? "v" : "V";
            case Sort::BOOL: return firstOrder ? "b" : "B";
            case Sort::DATA: return firstOrder ? "d" : "D";
            case Sort::PTR: return firstOrder ? "a" : "A";
        }
    }

    static inline std::string MakeSuffix() {
        return std::to_string(variables.size() + flows.size());
    }

    template<typename T>
    [[nodiscard]] static const T &MakeFreshSymbol(const Type &type, bool firstOrder,
                                                  std::deque<std::unique_ptr<T>> &container) {
        std::string newName = std::string(PREFIX) + MakeName(type, firstOrder) + MakeSuffix();
        auto newVariable = std::make_unique<T>(newName, type, false);
        container.push_back(std::move(newVariable));
        return *container.back();
    }

public:
    [[nodiscard]] static const SymbolicVariableDeclaration &MakeFreshVariable(const Type &type) {
        return MakeFreshSymbol(type, true, variables);
    }

    [[nodiscard]] static const SymbolicFlowDeclaration &MakeFreshFlow(const Type &type) {
        return MakeFreshSymbol(type, false, flows);
    }

    [[nodiscard]] static const decltype(variables) &GetAll() {
        return variables;
    }

    [[nodiscard]] static const decltype(flows) &GetFlows() {
        return flows;
    }
};

std::deque<std::unique_ptr<SymbolicVariableDeclaration>> SymbolicVariablePoolImpl::variables = std::deque<std::unique_ptr<SymbolicVariableDeclaration>>();
std::deque<std::unique_ptr<SymbolicFlowDeclaration>> SymbolicVariablePoolImpl::flows = std::deque<std::unique_ptr<SymbolicFlowDeclaration>>();

const SymbolicVariableDeclaration &SymbolicPool::MakeFreshVariable(const cola::Type &type) {
    return SymbolicVariablePoolImpl::MakeFreshVariable(type);
}

const std::deque<std::unique_ptr<SymbolicVariableDeclaration>> &SymbolicPool::GetAllVariables() {
    return SymbolicVariablePoolImpl::GetAll();
}

const SymbolicFlowDeclaration &SymbolicPool::MakeFreshFlow(const cola::Type &type) {
    return SymbolicVariablePoolImpl::MakeFreshFlow(type);
}

const std::deque<std::unique_ptr<SymbolicFlowDeclaration>> &SymbolicPool::GetAllFlows() {
    return SymbolicVariablePoolImpl::GetFlows();
}


SymbolicVariable::SymbolicVariable(const SymbolicVariableDeclaration &decl) : decl_storage(decl) {
}

SeparatingConjunction::SeparatingConjunction() = default;

SeparatingConjunction::SeparatingConjunction(std::deque<std::unique_ptr<Formula>> conjuncts_) : conjuncts(
        std::move(conjuncts_)) {
    for ([[maybe_unused]] const auto &conjunct : conjuncts) assert(conjunct);
}

SeparatingImplication::SeparatingImplication(std::unique_ptr<Formula> premise_, std::unique_ptr<Formula> conclusion_)
        : premise(std::move(premise_)), conclusion(std::move(conclusion_)) {
    assert(premise);
    assert(conclusion);
}

PointsToAxiom::PointsToAxiom(std::unique_ptr<SymbolicVariable> node_,
                             bool isLocal_, const SymbolicFlowDeclaration &flow_,
                             std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue_)
        : node(std::move(node_)), isLocal(isLocal_), flow(flow_), fieldToValue(std::move(fieldToValue_)) {
    assert(node);
    for ([[maybe_unused]] const auto &selector : node->Type().fields) {
        [[maybe_unused]] auto find = fieldToValue.find(selector.first);
        assert(find != fieldToValue.end());
        assert(find->second);
    }
}

EqualsToAxiom::EqualsToAxiom(std::unique_ptr<VariableExpression> variable_, std::unique_ptr<SymbolicVariable> value_)
        : variable(std::move(variable_)), value(std::move(value_)) {
    assert(variable);
    assert(value);
}

SymbolicAxiom::SymbolicAxiom(std::unique_ptr<SymbolicExpression> lhs_, Operator op,
                             std::unique_ptr<SymbolicExpression> rhs_)
        : lhs(std::move(lhs_)), op(op), rhs(std::move(rhs_)) {
    assert(lhs);
    assert(lhs);
    assert(lhs->Type() == rhs->Type() || cola::assignable(lhs->Type(), rhs->Type()) || cola::assignable(rhs->Type(), lhs->Type()));
}

StackDisjunction::StackDisjunction() = default;

StackDisjunction::StackDisjunction(std::deque<std::unique_ptr<SymbolicAxiom>> axioms_) : axioms(std::move(axioms_)) {
    for ([[maybe_unused]] const auto &axiom : axioms) assert(axiom);
}

InflowContainsValueAxiom::InflowContainsValueAxiom(const SymbolicFlowDeclaration &flow_,
                                                   std::unique_ptr<SymbolicVariable> value_)
        : flow(flow_), value(std::move(value_)) {
    assert(value);
    assert(flow_.type == value->Type());
}

InflowContainsRangeAxiom::InflowContainsRangeAxiom(const SymbolicFlowDeclaration& flow_,
                                                   std::unique_ptr<SymbolicExpression> valueLow_,
                                                   std::unique_ptr<SymbolicExpression> valueHigh_)
        : flow(flow_), valueLow(std::move(valueLow_)), valueHigh(std::move(valueHigh_)) {
    assert(valueLow);
    assert(valueHigh);
    assert(valueLow->Type() == valueHigh->Type());
    assert(flow_.type == valueLow->Type());
}

InflowEmptinessAxiom::InflowEmptinessAxiom(const SymbolicFlowDeclaration &flow, bool isEmpty)
        : flow(flow), isEmpty(isEmpty) {
}

SpecificationAxiom::SpecificationAxiom(Kind kind_, std::unique_ptr<SymbolicVariable> key_) : kind(kind_),
                                                                                             key(std::move(key_)) {
    assert(key);
    assert(key->Sort() == Sort::DATA);
}

ObligationAxiom::ObligationAxiom(Kind kind_, std::unique_ptr<SymbolicVariable> key_)
        : SpecificationAxiom(kind_, std::move(key_)) {
}

FulfillmentAxiom::FulfillmentAxiom(Kind kind_, std::unique_ptr<SymbolicVariable> key_, bool return_value_)
        : SpecificationAxiom(kind_, std::move(key_)), return_value(return_value_) {
}

PastPredicate::PastPredicate(std::unique_ptr<Formula> formula_) : formula(std::move(formula_)) {
    assert(formula);
}

FuturePredicate::FuturePredicate(std::unique_ptr<Formula> pre_, const cola::Assignment &command,
                                 std::unique_ptr<Formula> post_)
        : pre(std::move(pre_)), command(command), post(std::move(post_)) {
    assert(pre);
    assert(post);
}

Annotation::Annotation() : now(std::make_unique<SeparatingConjunction>()) {
}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> formula_) : now(std::move(formula_)) {
    assert(now);
}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> formula_,
                       std::deque<std::unique_ptr<TimePredicate>> time_) : now(std::move(formula_)),
                                                                           time(std::move(time_)) {
    assert(now);
    for ([[maybe_unused]] const auto &predicate : time) assert(predicate);
}
