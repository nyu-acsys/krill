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
constexpr std::string_view SYMBOL_STAR = "   *   ";
constexpr std::string_view SYMBOL_LOCAL_POINTS_TO = " |-> L";
constexpr std::string_view SYMBOL_SHARED_POINTS_TO = " |=> G";
constexpr std::string_view SYMBOL_EQUALS_TO = " ~~> ";
constexpr std::string_view SYMBOL_ELEMENT_OF = " ∈ ";
constexpr std::string_view SYMBOL_SUBSET = " ⊆ ";
constexpr std::string_view SYMBOL_OP_SPACE = " ";
constexpr std::string_view SYMBOL_EMPTY_SET = "∅";
constexpr std::string_view LITERAL_OBLIGATION = "@OBL";
constexpr std::string_view LITERAL_FULFILLMENT = "@FUL";
constexpr std::string_view SYMBOL_IMPLICATION = " --* ";
constexpr std::string_view SYMBOL_PAST = "PAST";
constexpr std::string_view SYMBOL_FUTURE = "FUT";
constexpr std::string_view SYMBOL_TIME_LEFT = "<< ";
constexpr std::string_view SYMBOL_TIME_RIGHT = " >>";


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

    void Visit(const SeparatingConjunction& formula) override {
        if (formula.conjuncts.empty()) {
            stream << LITERAL_TRUE;
            return;
        }
        bool first = true;
        for (const auto& elem : formula.conjuncts) {
            if (first) first = false;
            else stream << SYMBOL_STAR;
            elem->Accept(*this);
        }
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
        stream << formula.Variable() << SYMBOL_EQUALS_TO;
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
        stream << formula.spec << "(";
        formula.key->Accept(*this);
        stream << ")";
    }
    void Visit(const FulfillmentAxiom& formula) override {
        stream << LITERAL_FULFILLMENT << "(" << (formula.returnValue ? LITERAL_TRUE : LITERAL_FALSE) << ")";
    }

    void Visit(const SeparatingImplication& formula) override {
        stream << "[";
        formula.premise->Accept(*this);
        stream << "]" << SYMBOL_IMPLICATION << "[";
        formula.conclusion->Accept(*this);
        stream << "]";
    }

    void Visit(const PastPredicate& predicate) override {
        stream << SYMBOL_PAST << SYMBOL_TIME_LEFT;
        predicate.formula->Accept(*this);
        stream << SYMBOL_TIME_RIGHT;
    }
    void Visit(const FuturePredicate& predicate) override {
        stream << SYMBOL_FUTURE << SYMBOL_TIME_LEFT;
        predicate.pre->Accept(*this);
        stream << SYMBOL_TIME_RIGHT << " ";
        stream << predicate.command;
        stream << " " << SYMBOL_TIME_LEFT;
        predicate.post->Accept(*this);
        stream << SYMBOL_TIME_RIGHT;
    }

    void Visit(const Annotation& annotation) override {
        stream << "{" << LB << INDENT ;
        annotation.now->Accept(*this);
        auto printTime = [this](const auto& container) {
            if (!container.empty()) {
                stream << SYMBOL_STAR;
                for (const auto& elem : container) {
                    stream << LB << INDENT;
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