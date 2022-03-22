#include "logics/util.hpp"

#include <sstream>

using namespace plankton;


constexpr std::string_view LB = "\n";
constexpr std::string_view INDENT = "    ";
constexpr std::string_view LITERAL_TRUE = "true";
constexpr std::string_view LITERAL_FALSE = "false";
constexpr std::string_view LITERAL_NULL = "nullptr";
constexpr std::string_view LITERAL_MIN = "-∞";
constexpr std::string_view LITERAL_MAX = "∞";
constexpr std::string_view LITERAL_SELF = "_self";
constexpr std::string_view LITERAL_SOME = "_some";
constexpr std::string_view LITERAL_UNLOCKED = "_none";
constexpr std::string_view SYMBOL_STAR = "   *   ";
constexpr std::string_view SYMBOL_LOCAL_POINTS_TO = " |-> L";
constexpr std::string_view SYMBOL_SHARED_POINTS_TO = " |=> G";
constexpr std::string_view SYMBOL_EQUALS_TO = " ~~> ";
constexpr std::string_view SYMBOL_ELEMENT_OF = " ∈ ";
constexpr std::string_view SYMBOL_SUBSET = " ⊆ ";
constexpr std::string_view SYMBOL_OP_SPACE = " ";
constexpr std::string_view SYMBOL_EMPTY_SET = "∅";
constexpr std::string_view SYMBOL_UPDATE = " =>> ";
constexpr std::string_view LITERAL_OBLIGATION = "@OBL";
constexpr std::string_view LITERAL_FULFILLMENT = "@FUL";
constexpr std::string_view SYMBOL_IMPLICATION = "==>";
constexpr std::string_view SYMBOL_IMPLICATION_OPEN = "{ ";
constexpr std::string_view SYMBOL_IMPLICATION_CLOSE = " }";
constexpr std::string_view SYMBOL_PAST_LEFT = "PAST<< ";
constexpr std::string_view SYMBOL_PAST_RIGHT = " >>";
constexpr std::string_view SYMBOL_FUTURE_LEFT = "FUT<< ";
constexpr std::string_view SYMBOL_FUTURE_GUARD = "  |  ";
constexpr std::string_view SYMBOL_FUTURE_RIGHT = " >>";


//
// Wrappers
//

template<typename T>
std::string MakeString(const T& object) {
    std::stringstream stream;
    stream << object;
    return stream.str();
}

std::string plankton::ToString(const LogicObject& object) { return MakeString(object); }
std::string plankton::ToString(const SymbolDeclaration& object) { return MakeString(object); }
std::string plankton::ToString(Specification object) { return MakeString(object); }


//
// Printing AST
//

struct LogicPrinter : public LogicVisitor {
    std::ostream& stream;
    explicit LogicPrinter(std::ostream& stream_) : stream(stream_) {}

    void Visit(const SymbolicVariable& expression) override { stream << expression.Decl().name; }
    void Visit(const SymbolicBool& expression) override { stream << (expression.value ? LITERAL_TRUE : LITERAL_FALSE); }
    void Visit(const SymbolicNull& /*expression*/) override { stream << LITERAL_NULL; }
    void Visit(const SymbolicMin& /*expression*/) override { stream << LITERAL_MIN; }
    void Visit(const SymbolicMax& /*expression*/) override { stream << LITERAL_MAX; }
    void Visit(const SymbolicSelfTid& /*expression*/) override { stream << LITERAL_SELF; }
    void Visit(const SymbolicSomeTid& /*expression*/) override { stream << LITERAL_SOME; }
    void Visit(const SymbolicUnlocked& /*expression*/) override { stream << LITERAL_UNLOCKED; }

    template<typename T>
    inline void PrintSequence(const T& container, std::string_view join, std::string_view empty) {
        if (container.empty()) {
            stream << empty;
            return;
        }
        bool first = true;
        for (const auto& elem : container) {
            if (first) first = false;
            else stream << join;
            if constexpr (std::is_base_of_v<LogicObject, typename T::value_type::element_type>) elem->Accept(*this);
            else plankton::Print(*elem, stream);
        }
    }
    template<typename T>
    inline void HandleSeparatedConjuncts(const T& formula) {
        PrintSequence(formula.conjuncts, SYMBOL_STAR, LITERAL_TRUE);
    }

    void Visit(const Guard& expression) override {
        PrintSequence(expression.conjuncts, SYMBOL_STAR, LITERAL_TRUE);
    }
    void Visit(const Update& expression) override {
        if (expression.fields.empty()) {
            stream << SYMBOL_EMPTY_SET;
        } else {
            PrintSequence(expression.fields, ", ", "");
            stream << SYMBOL_UPDATE;
            PrintSequence(expression.values, ", ", "");
        }
    }
    void Visit(const SeparatingConjunction& formula) override {
        HandleSeparatedConjuncts(formula);
    }
    void PrintMemory(const MemoryAxiom& formula, std::string_view symbol) {
        formula.node->Accept(*this);
        stream << symbol << "(inflow=";
        formula.flow->Accept(*this);
        for (const auto& [field, value] : formula.fieldToValue) {
            stream << ", " << field << "=";
            value->Accept(*this);
        }
        stream << ")";
    }
    void Visit(const LocalMemoryResource& formula) override { PrintMemory(formula, SYMBOL_LOCAL_POINTS_TO); }
    void Visit(const SharedMemoryCore& formula) override { PrintMemory(formula, SYMBOL_SHARED_POINTS_TO); }
    void Visit(const EqualsToAxiom& formula) override {
        stream << formula.Variable().name << SYMBOL_EQUALS_TO;
        formula.value->Accept(*this);
    }
    void Visit(const StackAxiom& formula) override {
        formula.lhs->Accept(*this);
        stream << SYMBOL_OP_SPACE << formula.op << SYMBOL_OP_SPACE;
        formula.rhs->Accept(*this);
    }
    void Visit(const InflowContainsValueAxiom& formula) override {
        formula.value->Accept(*this);
        stream << SYMBOL_ELEMENT_OF;
        formula.flow->Accept(*this);
    }
    void Visit(const InflowContainsRangeAxiom& formula) override {
        stream << "[";
        formula.valueLow->Accept(*this);
        stream << ", ";
        formula.valueHigh->Accept(*this);
        stream << "]" << SYMBOL_SUBSET;
        formula.flow->Accept(*this);
    }
    void Visit(const InflowEmptinessAxiom& formula) override {
        formula.flow->Accept(*this);
        stream << SYMBOL_OP_SPACE << (formula.isEmpty ? BinaryOperator::EQ : BinaryOperator::NEQ) << SYMBOL_OP_SPACE
               << SYMBOL_EMPTY_SET;
    }
    void Visit(const ObligationAxiom& formula) override {
        stream << LITERAL_OBLIGATION;
        stream << "(" << formula.spec << " ";
        formula.key->Accept(*this);
        stream << ")";
    }
    void Visit(const FulfillmentAxiom& formula) override {
        stream << LITERAL_FULFILLMENT << "(" << (formula.returnValue ? LITERAL_TRUE : LITERAL_FALSE) << ")";
    }

    void Visit(const NonSeparatingImplication& formula) override {
        if (!formula.premise->conjuncts.empty()) {
            stream << SYMBOL_IMPLICATION_OPEN;
            formula.premise->Accept(*this);
            stream << SYMBOL_IMPLICATION_CLOSE << SYMBOL_IMPLICATION;
        }
        stream << SYMBOL_IMPLICATION_OPEN;
        formula.conclusion->Accept(*this);
        stream << SYMBOL_IMPLICATION_CLOSE;
    }
    void Visit(const ImplicationSet& formula) override {
        HandleSeparatedConjuncts(formula);
    }

    void Visit(const PastPredicate& predicate) override {
        stream << SYMBOL_PAST_LEFT;
        predicate.formula->Accept(*this);
        stream << SYMBOL_PAST_RIGHT;
    }
    void Visit(const FuturePredicate& predicate) override {
        stream << SYMBOL_FUTURE_LEFT;
        predicate.update->Accept(*this);
        stream << SYMBOL_FUTURE_GUARD;
        predicate.guard->Accept(*this);
        stream << SYMBOL_FUTURE_RIGHT;
    }

    void Visit(const Annotation& annotation) override {
        stream << "{" << LB << INDENT ;
        annotation.now->Accept(*this);
        auto printTime = [this](const auto& container) {
            if (!container.empty()) {
                // stream << SYMBOL_STAR;
                for (const auto& elem : container) {
                    stream << SYMBOL_STAR << LB << INDENT;
                    elem->Accept(*this);
                }
            }
        };
        printTime(annotation.past);
        printTime(annotation.future);
        stream << LB << "}";
    }

};

void plankton::Print(const LogicObject& object, std::ostream& out) {
    LogicPrinter printer(out);
    object.Accept(printer);
}
