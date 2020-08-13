#include "heal/util.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace heal;


const std::string AND_STR = "  ∧  ";
const std::string EMPTY_STR = "true";


inline void print_spec(const SpecificationAxiom& spec, std::ostream& stream) {
	stream << heal::to_string(spec.kind) << "(";
	cola::print(*spec.key, stream);
	stream << ")";
}

template<typename T, typename U>
void print_elems(std::ostream& stream, const T& elems, U printer, std::string delim=AND_STR, std::string empty=EMPTY_STR) {
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
	FormulaPrinter(std::ostream& stream_) : stream(stream_) {}

	template<typename T>
	static void print(const T& obj, std::ostream& stream) {
		FormulaPrinter printer(stream);
		obj.accept(printer);
	}

	void visit(const ConjunctionFormula& formula) override {
		print_elems(stream, formula.conjuncts, [this](const auto& e){
			e->accept(*this);
		});
	}

	void visit(const AxiomConjunctionFormula& formula) override {
		print_elems(stream, formula.conjuncts, [this](const auto& e){
			e->accept(*this);
		});
	}

	void visit(const ImplicationFormula& formula) override {
		stream << "[";
		formula.premise->accept(*this);
		stream << "]==>";
		stream << "[";
		formula.conclusion->accept(*this);
		stream << "]";
	}

	void visit(const NegatedAxiom& formula) override {
		stream << "¬[";
		formula.axiom->accept(*this);
		stream << "]";
	}

	void visit(const ExpressionAxiom& formula) override {
		cola::print(*formula.expr, stream);
	}

	void visit(const OwnershipAxiom& formula) override {
		stream << "@owned(";
		cola::print(*formula.expr, stream);
		stream << ")";
	}

	void visit(const DataStructureLogicallyContainsAxiom& formula) override {
		stream << "@DScontains(";
		cola::print(*formula.value, stream);
		stream << ")";
	}

	void visit(const NodeLogicallyContainsAxiom& formula) override {
		stream << "@lcontains(";
		cola::print(*formula.node, stream);
		stream << ", ";
		cola::print(*formula.value, stream);
		stream << ")";
	}

	void visit(const KeysetContainsAxiom& formula) override {
		stream << "@KScontained(";
		cola::print(*formula.node, stream);
		stream << ", ";
		cola::print(*formula.value, stream);
		stream << ")";
	}

	void visit(const HasFlowAxiom& formula) override {
		stream << "@flow(";
		cola::print(*formula.expr, stream);
		stream << ")≠⊥";
	}

	void visit(const FlowContainsAxiom& formula) override {
		stream << "@flowContains(";
		cola::print(*formula.node, stream);
		stream << "[";
		cola::print(*formula.value_low, stream);
		stream << ", ";
		cola::print(*formula.value_high, stream);
		stream << "])";
	}

	void visit(const UniqueInflowAxiom& formula) override {
		stream << "@inflowUnique(";
		cola::print(*formula.node, stream);
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
		stream << std::endl << "  " + AND_STR << std::endl << "    ";
		print_elems(stream, annotation.time, [this](const auto& e){ e->accept(*this); });
		stream << std::endl << "}";
	}

};

void heal::print(const Formula& formula, std::ostream& stream) {
	FormulaPrinter::print(formula, stream);
}
