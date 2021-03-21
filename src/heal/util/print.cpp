#include "heal/util.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace heal;


static constexpr std::string_view STAR_STR = "  *  ";
static constexpr std::string_view OR_STR = "  ∨  ";
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

std::string heal::ToString(StackAxiom::Operator op) {
    switch (op) {
        case StackAxiom::EQ: return "=";
        case StackAxiom::NEQ: return "≠";
        case StackAxiom::GT: return ">";
        case StackAxiom::GEQ: return "≥";
        case StackAxiom::LT: return "<";
        case StackAxiom::LEQ: return "≤";
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

    void visit(const LogicVariable& expression) override {
	    stream << expression.Decl().name;
	}

    void visit(const SymbolicBool& expression) override {
        stream << (expression.value ? "true" : "false");
	}

    void visit(const SymbolicNull& /*expression*/) override {
        stream << "NULL";
	}

    void visit(const SymbolicMin& /*expression*/) override {
        stream << "MIN_VAL";
	}

    void visit(const SymbolicMax& /*expression*/) override {
        stream << "MAX_VAL";
	}

    void visit(const SeparatingConjunction& formula) override {
        print_elems(stream, formula.conjuncts, [this](const auto& e){
            e->accept(*this);
        });
    }

    void visit(const FlatSeparatingConjunction& formula) override {
        print_elems(stream, formula.conjuncts, [this](const auto& e){
            e->accept(*this);
        });
    }

	void visit(const SeparatingImplication& formula) override {
		stream << "[";
		formula.premise->accept(*this);
		stream << "]--*";
		stream << "[";
		formula.conclusion->accept(*this);
		stream << "]";
	}

	void visit(const NegatedAxiom& formula) override {
		stream << "¬";
		formula.axiom->accept(*this);
		stream << "";
	}

	void visit(const BoolAxiom& formula) override {
        stream << (formula.value ? "true" : "false");
	}

    void visit(const PointsToAxiom& formula) override {
        formula.node->accept(*this);
        stream << "." << formula.fieldname << "↦";
        formula.value->accept(*this);
	}

    void visit(const StackAxiom& formula) override {
        formula.lhs->accept(*this);
        stream << " " << heal::ToString(formula.op) << " ";
        formula.rhs->accept(*this);
	}

    void visit(const StackDisjunction& formula) override {
        stream << "[";
        print_elems(stream, formula.axioms, [this](const auto& e){
            e->accept(*this);
        }, OR_STR);
        stream << "]";
	}

	void visit(const OwnershipAxiom& formula) override {
		stream << "@owned(";
		formula.node->accept(*this);
		stream << ")";
	}

	void visit(const DataStructureLogicallyContainsAxiom& formula) override {
		stream << "@DScontains(";
        formula.value->accept(*this);
		stream << ")";
	}

	void visit(const NodeLogicallyContainsAxiom& formula) override {
		stream << "@lcontains(";
        formula.node->accept(*this);
		stream << ", ";
        formula.value->accept(*this);
		stream << ")";
	}

	void visit(const KeysetContainsAxiom& formula) override {
		stream << "@KScontained(";
        formula.node->accept(*this);
		stream << ", ";
        formula.value->accept(*this);
		stream << ")";
	}

	void visit(const HasFlowAxiom& formula) override {
		stream << "@flow(";
        formula.node->accept(*this);
		stream << ")≠⊥";
	}

	void visit(const FlowContainsAxiom& formula) override {
		stream << "@flowContains(";
        formula.node->accept(*this);
		stream << "[";
        formula.value_low->accept(*this);
		stream << ", ";
        formula.value_high->accept(*this);
		stream << "])";
	}

	void visit(const UniqueInflowAxiom& formula) override {
		stream << "@inflowUnique(";
        formula.node->accept(*this);
		stream << ")";
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
		cola::print(*predicate.command, stream);
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
