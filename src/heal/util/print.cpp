#include "heal/util.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace heal;


static constexpr std::string_view STAR_STR = "   *   ";
static constexpr std::string_view OR_STR = "  ∨  ";
static constexpr std::string_view EQ_STR = " == ";
static constexpr std::string_view NEQ_STR = " != "; // ≠
static constexpr std::string_view LT_STR = " < ";
static constexpr std::string_view LEQ_STR = " <= "; // ≤
static constexpr std::string_view GT_STR = " > ";
static constexpr std::string_view GEQ_STR = " >= "; // ≥
static constexpr std::string_view PTO_STR = " |-> "; // ↦
static constexpr std::string_view ETO_STR = " ~~> "; // ↦
static constexpr std::string_view IMP_STR = " --* ";
static constexpr std::string_view IN_STR = " ∈ ";
static constexpr std::string_view SUB_STR = " ⊆ ";
static constexpr std::string_view NONE_STR = "∅";
static constexpr std::string_view MIN_STR = "-∞"; // MIN_VAL
static constexpr std::string_view MAX_STR = "∞"; // MAX_VAL
static constexpr std::string_view EMPTY_STR = "true";


std::string heal::ToString(const LogicObject& object) {
    std::stringstream stream;
    heal::Print(object, stream);
    return stream.str();
}

std::string heal::ToString(SpecificationAxiom::Kind kind) {
    switch (kind) {
        case SpecificationAxiom::Kind::CONTAINS: return "contains";
        case SpecificationAxiom::Kind::INSERT: return "insert";
        case SpecificationAxiom::Kind::DELETE: return "delete";
    }
}

std::string heal::ToString(SymbolicAxiom::Operator op) {
    switch (op) {
        case SymbolicAxiom::EQ: return std::string(EQ_STR);
        case SymbolicAxiom::NEQ: return std::string(NEQ_STR);
        case SymbolicAxiom::GT: return std::string(GT_STR);
        case SymbolicAxiom::GEQ: return std::string(GEQ_STR);
        case SymbolicAxiom::LT: return std::string(LT_STR);
        case SymbolicAxiom::LEQ: return std::string(LEQ_STR);
    }
}


inline void print_spec(const SpecificationAxiom& spec, std::ostream& stream) {
	stream << heal::ToString(spec.kind) << "(";
	heal::Print(*spec.key, stream);
	stream << ")";
}

template<typename T, typename U>
void print_elems(std::ostream& stream, const T& elems, U printer, std::string_view delim=STAR_STR, std::string_view empty=EMPTY_STR) {
	if (elems.empty()) {
		stream << empty;
	} else {
		bool first = true;
		for (const auto& elem : elems) {
			if (first) {
				first = false;
			} else {
				stream << delim;
			}
			printer(elem);
		}
	}
}

struct FormulaPrinter : public LogicVisitor {
	std::ostream& stream;
	explicit FormulaPrinter(std::ostream& stream_) : stream(stream_) {}

    void visit(const SymbolicVariable& expression) override {
	    stream << expression.Decl().name;
	}

    void visit(const SymbolicBool& expression) override {
        stream << (expression.value ? "true" : "false");
	}

    void visit(const SymbolicNull& /*expression*/) override {
        stream << "NULL";
	}

    void visit(const SymbolicMin& /*expression*/) override {
        stream << MIN_STR;
	}

    void visit(const SymbolicMax& /*expression*/) override {
        stream << MAX_STR;
	}

    void visit(const SeparatingConjunction& formula) override {
        print_elems(stream, formula.conjuncts, [this](const auto& e){
            e->accept(*this);
        });
    }

	void visit(const SeparatingImplication& formula) override {
		stream << "[";
		formula.premise->accept(*this);
		stream << "]" << IMP_STR << "[";
		formula.conclusion->accept(*this);
		stream << "]";
	}

    void visit(const PointsToAxiom& formula) override {
        formula.node->accept(*this);
        stream << PTO_STR << (formula.isLocal ? "L" : "G") << "(";
        stream << formula.flow.get().name;
        for (const auto& [field, value] : formula.fieldToValue) {
            stream << ", " << field << ":";
            value->accept(*this);
        }
        stream << ")";
	}

    void visit(const EqualsToAxiom& formula) override {
	    cola::print(*formula.variable, stream);
        stream << ETO_STR;
        formula.value->accept(*this);
    }

    void visit(const SymbolicAxiom& formula) override {
        formula.lhs->accept(*this);
        stream << heal::ToString(formula.op);
        formula.rhs->accept(*this);
	}

    void visit(const StackDisjunction& formula) override {
        stream << "[";
        print_elems(stream, formula.axioms, [this](const auto& e){
            e->accept(*this);
        }, OR_STR);
        stream << "]";
	}

    void visit(const InflowContainsValueAxiom& formula) override {
        formula.value->accept(*this);
        stream << IN_STR << formula.flow.get().name;
    }

    void visit(const InflowContainsRangeAxiom& formula) override {
        stream << "[";
        formula.valueLow->accept(*this);
        stream << ", ";
        formula.valueHigh->accept(*this);
        stream << "]" << SUB_STR << formula.flow.get().name;
    }

    void visit(const InflowEmptinessAxiom& formula) override {
	    stream << formula.flow.get().name << (formula.isEmpty ? EQ_STR : NEQ_STR) << NONE_STR;
    }

	void visit(const ObligationAxiom& formula) override {
		stream << "@OBL:";
		print_spec(formula, stream);
	}

	void visit(const FulfillmentAxiom& formula) override {
		stream << "@FUL:";
		print_spec(formula, stream);
		stream << "=" << (formula.return_value ? "true" : "false");
	}

	void visit(const PastPredicate& predicate) override {
		stream << "PAST<< ";
		predicate.formula->accept(*this);
		stream << " >>";
	}
	
	void visit(const FuturePredicate& predicate) override {
		stream << "FUT<< ";
		predicate.pre->accept(*this);
		stream << " >> ";
		cola::print(predicate.command, stream);
		stream << " << ";
		predicate.post->accept(*this);
		stream << " >>";
	}

	void visit(const Annotation& annotation) override {
		stream << "{" << std::endl << "    " ;
        annotation.now->accept(*this);
        if (!annotation.time.empty()) {
            stream << STAR_STR;
            print_elems(stream, annotation.time, [this](const auto &e) {
                stream << std::endl << "    " ;
                e->accept(*this);
            });
        }
		stream << std::endl << "}";
	}

};

void heal::Print(const LogicObject& object, std::ostream& stream) {
    FormulaPrinter printer(stream);
    object.accept(printer);
}


std::ostream& heal::operator<<(std::ostream& stream, const LogicObject& node) {
    heal::Print(node, stream);
    return stream;
}

std::ostream& heal::operator<<(std::ostream& stream, const SymbolicVariableDeclaration& decl) {
    stream << decl.name << ":" << decl.type.name;
    return stream;
}

std::ostream& heal::operator<<(std::ostream& stream, const SymbolicFlowDeclaration& decl) {
    stream << decl.name << ":{ " << decl.type.name << " }";
    return stream;
}

