#include "logics/ast.hpp"

#include <utility>

#include "logics/util.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


//
// Symbolic Variables
//

bool SymbolDeclaration::operator==(const SymbolDeclaration& other) const { return this == &other; }
bool SymbolDeclaration::operator!=(const SymbolDeclaration& other) const { return this != &other; }

inline constexpr std::string_view MakeNamePrefix(Sort sort, Order order) {
    switch (order) {
        case Order::FIRST:
            switch (sort) {
                case Sort::VOID: return "@v";
                case Sort::BOOL: return "@b";
                case Sort::DATA: return "@d";
                case Sort::TID: return "@t";
                case Sort::PTR: return "@a";
            }
            throw;
        case Order::SECOND:
            switch (sort) {
                case Sort::VOID: return "@V";
                case Sort::BOOL: return "@B";
                case Sort::DATA: return "@D";
                case Sort::TID: return "@T";
                case Sort::PTR: return "@A";
            }
            throw;
    }
    throw;
}

inline std::string MakeName(const Type& type, Order order, std::size_t counter) {
    std::string result(MakeNamePrefix(type.sort, order));
    result += std::to_string(counter);
    return result;
}

SymbolDeclaration::SymbolDeclaration(std::string name, const Type& type, Order order)
        : name(std::move(name)), type(type), order(order) {
}

SymbolFactory::SymbolFactory() = default;

SymbolFactory::SymbolFactory(const LogicObject& avoid) {
    Avoid(avoid);
}

void SymbolFactory::Avoid(const LogicObject& avoid) {
    auto symbols = plankton::Collect<SymbolDeclaration>(avoid);
    plankton::InsertInto(std::move(symbols), inUse);
}

void SymbolFactory::Avoid(const SymbolDeclaration& avoid) {
    inUse.insert(&avoid);
}

const SymbolDeclaration& SymbolFactory::GetFresh(const Type& type, Order order) {
    static std::deque<std::unique_ptr<SymbolDeclaration>> symbols;

    // try to find existing symbol
    // TODO: implement more efficiently
    auto find = FindIf(symbols, [this, &type, order](const auto& elem) {
        assert(elem);
        return elem->type == type && elem->order == order && inUse.count(elem.get()) == 0;
    });
    
    const SymbolDeclaration* result;
    if (find != symbols.end()) {
        result = find->get();
    } else {
        // make new symbol
        assert(order == Order::FIRST || type == Type::Data());
        symbols.emplace_back(new SymbolDeclaration(MakeName(type, order, symbols.size()), type, order));
        result = symbols.back().get();
    }
    
    assert(result);
    inUse.insert(result);
    return *result;
}


//
// Symbolic Expressions
//

SymbolicVariable::SymbolicVariable(const SymbolDeclaration& decl) : decl(decl) {}

Order SymbolicVariable::GetOrder() const { return Decl().order; }

const Type& SymbolicVariable::GetType() const { return Decl().type; }

SymbolicBool::SymbolicBool(bool value) : value(value) {}

Order SymbolicBool::GetOrder() const { return plankton::Order::FIRST; }

const Type& SymbolicBool::GetType() const { return Type::Bool(); }

SymbolicNull::SymbolicNull() = default;

Order SymbolicNull::GetOrder() const { return plankton::Order::FIRST; }

const Type& SymbolicNull::GetType() const { return Type::Null(); }

SymbolicMin::SymbolicMin() = default;

Order SymbolicMin::GetOrder() const { return plankton::Order::FIRST; }

const Type& SymbolicMin::GetType() const { return Type::Data(); }

SymbolicMax::SymbolicMax() = default;

Order SymbolicMax::GetOrder() const { return plankton::Order::FIRST; }

const Type& SymbolicMax::GetType() const { return Type::Data(); }

SymbolicSelfTid::SymbolicSelfTid() = default;

Order SymbolicSelfTid::GetOrder() const { return plankton::Order::FIRST; }

const Type& SymbolicSelfTid::GetType() const { return Type::Thread(); }

SymbolicSomeTid::SymbolicSomeTid() = default;

Order SymbolicSomeTid::GetOrder() const { return plankton::Order::FIRST; }

const Type& SymbolicSomeTid::GetType() const { return Type::Thread(); }

SymbolicUnlocked::SymbolicUnlocked() = default;

Order SymbolicUnlocked::GetOrder() const { return plankton::Order::FIRST; }

const Type& SymbolicUnlocked::GetType() const { return Type::Thread(); }


//
// Formulas
//

SeparatingConjunction::SeparatingConjunction() = default;

void SeparatingConjunction::Conjoin(std::unique_ptr<Formula> formula) {
    // TODO: flatten if formula is a SeparatingConjunction?
    conjuncts.push_back(std::move(formula));
}

void SeparatingConjunction::RemoveConjunctsIf(const std::function<bool(const Formula&)>& predicate) {
    Simplify(*this);
    plankton::RemoveIf(conjuncts, [&predicate](const auto& elem){ return predicate(*elem); });
}

MemoryAxiom::MemoryAxiom(std::unique_ptr<SymbolicVariable> adr, std::unique_ptr<SymbolicVariable> flw,
                         std::map<std::string, std::unique_ptr<SymbolicVariable>> fields)
        : node(std::move(adr)), flow(std::move(flw)), fieldToValue(std::move(fields)) {
    assert(node);
    assert(node->GetOrder() == Order::FIRST);
    assert(flow);
    assert(flow->GetOrder() == Order::SECOND);
    assert(plankton::All(node->GetType(), [this](const auto& field) {
        return fieldToValue.count(field.first) != 0
               && fieldToValue[field.first]->GetOrder() == Order::FIRST
               && fieldToValue[field.first]->GetType().AssignableTo(field.second);
    }));
}

std::map<std::string, std::unique_ptr<SymbolicVariable>> ToUPtr(const std::map<std::string, std::reference_wrapper<const SymbolDeclaration>>& fieldToValue) {
    std::map<std::string, std::unique_ptr<SymbolicVariable>> result;
    for (const auto& [field, value] : fieldToValue) result[field] = std::make_unique<SymbolicVariable>(value);
    return result;
}

MemoryAxiom::MemoryAxiom(const SymbolDeclaration& node, const SymbolDeclaration& flow,
                         const std::map<std::string, std::reference_wrapper<const SymbolDeclaration>>& fieldToValue)
        : MemoryAxiom(std::make_unique<SymbolicVariable>(node), std::make_unique<SymbolicVariable>(flow),
                      ToUPtr(fieldToValue)) {}
                      
EqualsToAxiom::EqualsToAxiom(const VariableDeclaration& var, std::unique_ptr<SymbolicVariable> val)
        : variable(var), value(std::move(val)) {
    assert(value);
    assert(variable.get().type == value->GetType());
    assert(value->GetOrder() == Order::FIRST);
}

EqualsToAxiom::EqualsToAxiom(const VariableDeclaration& variable, const SymbolDeclaration& value)
        : EqualsToAxiom(variable, std::make_unique<SymbolicVariable>(value)) {}

StackAxiom::StackAxiom(BinaryOperator op, std::unique_ptr<SymbolicExpression> left,
                       std::unique_ptr<SymbolicExpression> right)
        : op(op), lhs(std::move(left)), rhs(std::move(right)) {
    assert(lhs);
    assert(rhs);
    assert(rhs->GetType().Comparable(lhs->GetType()));
    assert(rhs->GetOrder() == lhs->GetOrder());
}

InflowEmptinessAxiom::InflowEmptinessAxiom(std::unique_ptr<SymbolicVariable> flw, bool isEmpty)
        : flow(std::move(flw)), isEmpty(isEmpty) {
    assert(flow);
    assert(flow->GetOrder() == Order::SECOND);
}

InflowEmptinessAxiom::InflowEmptinessAxiom(const SymbolDeclaration& flow, bool isEmpty)
        : InflowEmptinessAxiom(std::make_unique<SymbolicVariable>(flow), isEmpty) {}

InflowContainsValueAxiom::InflowContainsValueAxiom(std::unique_ptr<SymbolicVariable> flw,
                                                   std::unique_ptr<SymbolicVariable> val)
        : flow(std::move(flw)), value(std::move(val)) {
    assert(flow);
    assert(flow->GetOrder() == Order::SECOND);
    assert(value);
    assert(value->GetOrder() == Order::FIRST);
    assert(flow->GetType() == value->GetType());
}

InflowContainsValueAxiom::InflowContainsValueAxiom(const SymbolDeclaration& flow, const SymbolDeclaration& value)
        : InflowContainsValueAxiom(std::make_unique<SymbolicVariable>(flow),
                                   std::make_unique<SymbolicVariable>(value)) {}

InflowContainsRangeAxiom::InflowContainsRangeAxiom(std::unique_ptr<SymbolicVariable> flw,
                                                   std::unique_ptr<SymbolicExpression> low,
                                                   std::unique_ptr<SymbolicExpression> high)
        : flow(std::move(flw)), valueLow(std::move(low)), valueHigh(std::move(high)) {
    assert(flow);
    assert(flow->GetOrder() == Order::SECOND);
    assert(valueLow);
    assert(valueLow->GetOrder() == Order::FIRST);
    assert(flow->GetType() == valueLow->GetType());
    assert(valueHigh);
    assert(valueHigh->GetOrder() == Order::FIRST);
    assert(valueLow->GetType() == valueHigh->GetType());
}

InflowContainsRangeAxiom::InflowContainsRangeAxiom(const SymbolDeclaration& flow,
                                                   const SymbolDeclaration& valueLow,
                                                   const SymbolDeclaration& valueHigh)
        : InflowContainsRangeAxiom(std::make_unique<SymbolicVariable>(flow),
                                   std::make_unique<SymbolicVariable>(valueLow),
                                   std::make_unique<SymbolicVariable>(valueHigh)) {}

ObligationAxiom::ObligationAxiom(Specification spec, std::unique_ptr<SymbolicVariable> k) : spec(spec), key(std::move(k)) {
    assert(key);
    assert(key->GetOrder() == Order::FIRST);
    assert(key->GetType() == Type::Data());
}

ObligationAxiom::ObligationAxiom(Specification spec, const SymbolDeclaration& key)
        : ObligationAxiom(spec, std::make_unique<SymbolicVariable>(key)) {}

FulfillmentAxiom::FulfillmentAxiom(bool returnValue) : returnValue(returnValue) {}


//
// Invariants
//

NonSeparatingImplication::NonSeparatingImplication()
        : premise(std::make_unique<SeparatingConjunction>()), conclusion(std::make_unique<SeparatingConjunction>()) {
}

NonSeparatingImplication::NonSeparatingImplication(std::unique_ptr<Formula> pre, std::unique_ptr<Formula> post)
        : premise(std::make_unique<SeparatingConjunction>()), conclusion(std::make_unique<SeparatingConjunction>()) {
    assert(pre);
    assert(post);
    premise->Conjoin(std::move(pre));
    conclusion->Conjoin(std::move(post));
    plankton::Simplify(*premise);
    plankton::Simplify(*conclusion);
}

ImplicationSet::ImplicationSet() = default;

void ImplicationSet::Conjoin(std::unique_ptr<NonSeparatingImplication> implication) {
    conjuncts.push_back(std::move(implication));
}


//
// Predicates
//

PastPredicate::PastPredicate(std::unique_ptr<SharedMemoryCore> form) : formula(std::move(form)) {
    assert(formula);
}

Guard::Guard() = default;

Guard::Guard(std::unique_ptr<BinaryExpression> expression_) {
    assert(expression_);
    conjuncts.push_back(std::move(expression_));
}

Guard::Guard(std::deque<std::unique_ptr<BinaryExpression>> conjuncts_) : conjuncts(std::move(conjuncts_)) {
    assert(plankton::AllNonNull(conjuncts));
}

Update::Update() = default;

Update::Update(std::unique_ptr<Dereference> field_, std::unique_ptr<SymbolicExpression> value_) {
    assert(field_);
    assert(value_);
    assert(field_->GetType().AssignableFrom(value_->GetType()));
    fields.push_back(std::move(field_));
    values.push_back(std::move(value_));
}

bool CheckUpdates(const std::vector<std::unique_ptr<Dereference>>& fields, const std::vector<std::unique_ptr<SymbolicExpression>>& values) {
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (fields.at(index)->GetType().AssignableFrom(values.at(index)->GetType())) continue;
        return false;
    }
    return true;
}

Update::Update(std::vector<std::unique_ptr<Dereference>> fields_, std::vector<std::unique_ptr<SymbolicExpression>> values_) : fields(std::move(fields_)), values(std::move(values_)) {
    assert(plankton::AllNonNull(fields));
    assert(plankton::AllNonNull(values));
    assert(fields.size() == values.size());
    assert(CheckUpdates(fields, values));
}

FuturePredicate::FuturePredicate(std::unique_ptr<Update> update_, std::unique_ptr<Guard> guard_)
        : guard(std::move(guard_)), update(std::move(update_)) {
    assert(guard);
    assert(update);
}


//
// Annotation
//
Annotation::Annotation() : now(std::make_unique<SeparatingConjunction>()) {}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> nw) : now(std::move(nw)) {
    assert(now);
}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> nw, std::deque<std::unique_ptr<PastPredicate>> pst,
                       std::deque<std::unique_ptr<FuturePredicate>> ftr)
                       : now(std::move(nw)), past(std::move(pst)), future(std::move(ftr)) {
    assert(now);
    assert(plankton::All(past, [](const auto& elem){ return elem.get() != nullptr; }));
    assert(plankton::All(future, [](const auto& elem){ return elem.get() != nullptr; }));
}

void Annotation::Conjoin(std::unique_ptr<Formula> formula) { now->Conjoin(std::move(formula)); }
void Annotation::Conjoin(std::unique_ptr<PastPredicate> predicate) { past.push_back(std::move(predicate)); }
void Annotation::Conjoin(std::unique_ptr<FuturePredicate> predicate) { future.push_back(std::move(predicate)); }


//
// Output
//

std::ostream& plankton::operator<<(std::ostream& out, const LogicObject& node) {
    plankton::Print(node, out);
    return out;
}

std::ostream& plankton::operator<<(std::ostream& out, const SymbolDeclaration& decl) {
    out << decl.name;
    return out;
}

std::ostream& plankton::operator<<(std::ostream& out, Specification spec) {
    switch (spec) {
        case Specification::CONTAINS: out << "contains"; break;
        case Specification::INSERT: out << "insert"; break;
        case Specification::DELETE: out << "delete"; break;
    }
    return out;
}
