#include "logics/ast.hpp"

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
                case Sort::PTR: return "@a";
            }
            case Order::SECOND:
                switch (sort) {
                    case Sort::VOID: return "@V";
                    case Sort::BOOL: return "@B";
                    case Sort::DATA: return "@D";
                    case Sort::PTR: return "@A";
                }
    }
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

Order SymbolicVariable::Order() const { return Decl().order; }

const Type& SymbolicVariable::Type() const { return Decl().type; }

SymbolicBool::SymbolicBool(bool value) : value(value) {}

Order SymbolicBool::Order() const { return plankton::Order::FIRST; }

const Type& SymbolicBool::Type() const { return Type::Bool(); }

SymbolicNull::SymbolicNull() = default;

Order SymbolicNull::Order() const { return plankton::Order::FIRST; }

const Type& SymbolicNull::Type() const { return Type::Null(); }

SymbolicMin::SymbolicMin() = default;

Order SymbolicMin::Order() const { return plankton::Order::FIRST; }

const Type& SymbolicMin::Type() const { return Type::Data(); }

SymbolicMax::SymbolicMax() = default;

Order SymbolicMax::Order() const { return plankton::Order::FIRST; }

const Type& SymbolicMax::Type() const { return Type::Data(); }


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
    RemoveIf(conjuncts, [&predicate](const auto& elem){ return predicate(*elem); });
}

MemoryAxiom::MemoryAxiom(std::unique_ptr<SymbolicVariable> adr, std::unique_ptr<SymbolicVariable> flw,
                         std::map<std::string, std::unique_ptr<SymbolicVariable>> fields)
        : node(std::move(adr)), flow(std::move(flw)), fieldToValue(std::move(fields)) {
    assert(node);
    assert(node->Order() == Order::FIRST);
    assert(flow);
    assert(flow->Order() == Order::SECOND);
    for (const auto& field : node->Type()) {
        assert(fieldToValue.count(field.first) != 0);
        assert(fieldToValue[field.first]->Order() == Order::FIRST);
        assert(fieldToValue[field.first]->Type() == field.second);
    }
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
    assert(variable.get().type == value->Type());
    assert(value->Order() == Order::FIRST);
}

EqualsToAxiom::EqualsToAxiom(const VariableDeclaration& variable, const SymbolDeclaration& value)
        : EqualsToAxiom(variable, std::make_unique<SymbolicVariable>(value)) {}

StackAxiom::StackAxiom(BinaryOperator op, std::unique_ptr<SymbolicExpression> left,
                       std::unique_ptr<SymbolicExpression> right)
        : op(op), lhs(std::move(left)), rhs(std::move(right)) {
    assert(lhs);
    assert(rhs);
    assert(rhs->Type().Comparable(lhs->Type()));
    assert(rhs->Order() == lhs->Order());
}

InflowEmptinessAxiom::InflowEmptinessAxiom(std::unique_ptr<SymbolicVariable> flw, bool isEmpty)
        : flow(std::move(flw)), isEmpty(isEmpty) {
    assert(flow);
    assert(flow->Order() == Order::SECOND);
}

InflowEmptinessAxiom::InflowEmptinessAxiom(const SymbolDeclaration& flow, bool isEmpty)
        : InflowEmptinessAxiom(std::make_unique<SymbolicVariable>(flow), isEmpty) {}

InflowContainsValueAxiom::InflowContainsValueAxiom(std::unique_ptr<SymbolicVariable> flw,
                                                   std::unique_ptr<SymbolicVariable> val)
        : flow(std::move(flw)), value(std::move(val)) {
    assert(flow);
    assert(flow->Order() == Order::SECOND);
    assert(value);
    assert(value->Order() == Order::FIRST);
    assert(flow->Type() == value->Type());
}

InflowContainsValueAxiom::InflowContainsValueAxiom(const SymbolDeclaration& flow, const SymbolDeclaration& value)
        : InflowContainsValueAxiom(std::make_unique<SymbolicVariable>(flow),
                                   std::make_unique<SymbolicVariable>(value)) {}

InflowContainsRangeAxiom::InflowContainsRangeAxiom(std::unique_ptr<SymbolicVariable> flw,
                                                   std::unique_ptr<SymbolicExpression> low,
                                                   std::unique_ptr<SymbolicExpression> high)
        : flow(std::move(flw)), valueLow(std::move(low)), valueHigh(std::move(high)) {
    assert(flow);
    assert(flow->Order() == Order::SECOND);
    assert(valueLow);
    assert(valueLow->Order() == Order::FIRST);
    assert(flow->Type() == valueLow->Type());
    assert(valueHigh);
    assert(valueHigh->Order() == Order::FIRST);
    assert(valueLow->Type() == valueHigh->Type());
}

InflowContainsRangeAxiom::InflowContainsRangeAxiom(const SymbolDeclaration& flow,
                                                   const SymbolDeclaration& valueLow,
                                                   const SymbolDeclaration& valueHigh)
        : InflowContainsRangeAxiom(std::make_unique<SymbolicVariable>(flow),
                                   std::make_unique<SymbolicVariable>(valueLow),
                                   std::make_unique<SymbolicVariable>(valueHigh)) {}

ObligationAxiom::ObligationAxiom(Specification spec, std::unique_ptr<SymbolicVariable> k) : spec(spec), key(std::move(k)) {
    assert(key);
    assert(key->Order() == Order::FIRST);
    assert(key->Type() == Type::Data());
}

ObligationAxiom::ObligationAxiom(Specification spec, const SymbolDeclaration& key)
        : ObligationAxiom(spec, std::make_unique<SymbolicVariable>(key)) {}

FulfillmentAxiom::FulfillmentAxiom(bool returnValue) : returnValue(returnValue) {}


//
// Invariants
//

SeparatingImplication::SeparatingImplication()
        : premise(std::make_unique<SeparatingConjunction>()), conclusion(std::make_unique<SeparatingConjunction>()) {
}

SeparatingImplication::SeparatingImplication(std::unique_ptr<Formula> pre, std::unique_ptr<Formula> post)
        : premise(std::make_unique<SeparatingConjunction>()), conclusion(std::make_unique<SeparatingConjunction>()) {
    assert(pre);
    assert(post);
    premise->Conjoin(std::move(pre));
    conclusion->Conjoin(std::move(post));
    plankton::Simplify(*premise);
    plankton::Simplify(*conclusion);
}

Invariant::Invariant() = default;

void Invariant::Conjoin(std::unique_ptr<SeparatingImplication> implication) {
    conjuncts.push_back(std::move(implication));
}


//
// Predicates
//

PastPredicate::PastPredicate(std::unique_ptr<SharedMemoryCore> form) : formula(std::move(form)) {
    assert(formula);
}

FuturePredicate::FuturePredicate(const MemoryWrite& cmd, std::unique_ptr<Formula> left, std::unique_ptr<Formula> right)
        : command(cmd), pre(std::move(left)), post(std::move(right)) {
    assert(pre);
    assert(post);
}


//
// Annotation
//
Annotation::Annotation() : now(std::make_unique<SeparatingConjunction>()) {}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> nw) : now(std::move(nw)) {
    assert(nw);
}

Annotation::Annotation(std::unique_ptr<SeparatingConjunction> nw, std::deque<std::unique_ptr<PastPredicate>> pst,
                       std::deque<std::unique_ptr<FuturePredicate>> ftr)
                       : now(std::move(nw)), past(std::move(pst)), future(std::move(ftr)) {
    assert(now);
    for (const auto& elem : past) assert(elem);
    for (const auto& elem : future) assert(elem);
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
