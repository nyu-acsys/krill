#include "logics/ast.hpp"

#include "logics/util.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


//
// Symbolic Variables
//

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

inline std::string MakeName(const Type& type, Order order) {
    std::string result(MakeNamePrefix(type.sort, order));
    result += "";
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

//        struct Comparator {
//            inline bool operator()(std::unique_ptr<SymbolDeclaration> symbol, std::unique_ptr<SymbolDeclaration> other) {
//                return symbol.get() < other.get();
//            }
//        };
//        // use: std::set<std::unique_ptr<SymbolDeclaration>, Comparator> symbols;
//        assert(inUse.size() <= symbols.size());
//        auto symbolIter = symbols.begin();
//        auto symbolEnd = symbols.end();
//        auto useIter = inUse.begin();
//        while (symbolIter != symbolEnd && symbolIter->get() == *useIter) {
//            ++symbolIter;
//            ++useIter;
//        }
//        if (symbolIter == symbolEnd) return nullptr;
//        else return symbolIter->get();

const SymbolDeclaration& SymbolFactory::GetFresh(const Type& type, Order order) {
    static std::deque<std::unique_ptr<SymbolDeclaration>> symbols;
    
    // try to find existing symbol
    // TODO: implement more efficiently
    auto find = FindIf(symbols, [this](const auto& elem) { return inUse.count(elem.get()) == 0; });
    if (find != symbols.end()) return **find;
    
    // make new symbol
    symbols.emplace_back(new SymbolDeclaration(MakeName(type, order), type, order));
    return *symbols.back();
}


//
// Symbolic Expressions
//

SymbolicVariable::SymbolicVariable(const SymbolDeclaration& decl) : decl(decl) {}

const Type& SymbolicVariable::Type() const { return Decl().type; }

SymbolicBool::SymbolicBool(bool value) : value(value) {}

const Type& SymbolicBool::Type() const { return Type::Bool(); }

SymbolicNull::SymbolicNull(const plankton::Type& type) : type(type) {}

const Type& SymbolicNull::Type() const { return type; }

SymbolicMin::SymbolicMin() = default;

const Type& SymbolicMin::Type() const { return Type::Data(); }

SymbolicMax::SymbolicMax() = default;

const Type& SymbolicMax::Type() const { return Type::Data(); }


//
// Formulas
//

SeparatingConjunction::SeparatingConjunction() = default;

MemoryAxiom::MemoryAxiom(std::unique_ptr<SymbolicVariable> adr, std::unique_ptr<SymbolicVariable> flw,
                         std::map<std::string, std::unique_ptr<SymbolicVariable>> fields)
        : node(std::move(adr)), flow(std::move(flw)), fieldToValue(std::move(fields)) {
    assert(adr);
    assert(flw);
    for (const auto& field : adr->Type()) assert(fields.count(field.first) != 0);
}

EqualsToAxiom::EqualsToAxiom(std::unique_ptr<VariableExpression> var, std::unique_ptr<SymbolicVariable> val)
        : variable(std::move(var)), value(std::move(val)) {
    assert(variable);
    assert(value);
}

StackAxiom::StackAxiom(BinaryOperator op, std::unique_ptr<SymbolicExpression> left,
                       std::unique_ptr<SymbolicExpression> right)
        : op(op), lhs(std::move(left)), rhs(std::move(right)) {
    assert(lhs);
    assert(rhs);
}

InflowEmptinessAxiom::InflowEmptinessAxiom(std::unique_ptr<SymbolicVariable> flw, bool isEmpty) : flow(std::move(flw)),
                                                                                                  isEmpty(isEmpty) {
    assert(flow);
}

InflowContainsValueAxiom::InflowContainsValueAxiom(std::unique_ptr<SymbolicVariable> flw,
                                                   std::unique_ptr<SymbolicVariable> val)
        : flow(std::move(flw)), value(std::move(val)) {
    assert(flow);
    assert(value);
}

InflowContainsRangeAxiom::InflowContainsRangeAxiom(std::unique_ptr<SymbolicVariable> flw,
                                                   std::unique_ptr<SymbolicExpression> low,
                                                   std::unique_ptr<SymbolicExpression> high)
        : flow(std::move(flw)), valueLow(std::move(low)), valueHigh(std::move(high)) {
    assert(flw);
    assert(valueLow);
    assert(valueHigh);
}

ObligationAxiom::ObligationAxiom(Kind kind, std::unique_ptr<SymbolicVariable> k) : kind(kind), key(std::move(k)) {
    assert(key);
}

FulfillmentAxiom::FulfillmentAxiom(bool returnValue) : returnValue(returnValue) {}


//
// Implications
//

SeparatingImplication::SeparatingImplication(std::unique_ptr<Formula> pre, std::unique_ptr<Formula> post)
: premise(std::move(pre)), conclusion(std::move(post)) {
    assert(premise);
    assert(conclusion);
}


//
// Predicates
//

PastPredicate::PastPredicate(std::unique_ptr<Formula> form) : formula(std::move(form)) {
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

std::ostream& plankton::operator<<(std::ostream& out, ObligationAxiom::Kind kind) {
    switch (kind) {
        case ObligationAxiom::Kind::CONTAINS: out << "contains"; break;
        case ObligationAxiom::Kind::INSERT: out << "insert"; break;
        case ObligationAxiom::Kind::DELETE: out << "delete"; break;
    }
    return out;
}
