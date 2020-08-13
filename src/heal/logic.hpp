#pragma once
#ifndef HEAL_LOGIC
#define HEAL_LOGIC


#include <array>
#include <deque>
#include <memory>
#include "cola/ast.hpp"
#include "heal/visitors.hpp"


namespace heal {

	//
	// Compound Formulas
	//

	struct Formula {
		Formula(const Formula& other) = delete;
		virtual ~Formula() = default;
		virtual void accept(LogicVisitor& visitor) const = 0;
		virtual void accept(LogicNonConstVisitor& visitor) = 0;
		protected: Formula() {}
	};

	#define ACCEPT_FORMULA_VISITOR \
		virtual void accept(LogicNonConstVisitor& visitor) override { visitor.visit(*this); } \
		virtual void accept(LogicVisitor& visitor) const override { visitor.visit(*this); }

	//
	// Compound Formulas
	//

	struct NowFormula : public Formula {
	};

	struct SimpleFormula : public NowFormula {
	};

	struct Axiom : public SimpleFormula {
	};

	struct AxiomConjunctionFormula : public NowFormula {
		std::deque<std::unique_ptr<Axiom>> conjuncts;
		
		decltype(conjuncts)::iterator begin() { return conjuncts.begin(); }
		decltype(conjuncts)::iterator end() { return conjuncts.end(); }
		decltype(conjuncts)::const_iterator begin() const { return conjuncts.cbegin(); }
		decltype(conjuncts)::const_iterator end() const { return conjuncts.cend(); }
		ACCEPT_FORMULA_VISITOR
	};

	struct ImplicationFormula : public SimpleFormula {
		std::unique_ptr<AxiomConjunctionFormula> premise;
		std::unique_ptr<AxiomConjunctionFormula> conclusion;

		ImplicationFormula();
		ACCEPT_FORMULA_VISITOR
	};

	struct ConjunctionFormula : public NowFormula {
		std::deque<std::unique_ptr<SimpleFormula>> conjuncts;

		decltype(conjuncts)::iterator begin() { return conjuncts.begin(); }
		decltype(conjuncts)::iterator end() { return conjuncts.end(); }
		decltype(conjuncts)::const_iterator begin() const { return conjuncts.cbegin(); }
		decltype(conjuncts)::const_iterator end() const { return conjuncts.cend(); }
		ACCEPT_FORMULA_VISITOR
	};

	//
	// Axioms
	//

	struct NegatedAxiom : public Axiom {
		std::unique_ptr<Axiom> axiom;

		NegatedAxiom(std::unique_ptr<Axiom> axiom);
		ACCEPT_FORMULA_VISITOR
	};

	struct ExpressionAxiom : public Axiom {
		std::unique_ptr<cola::Expression> expr;

		ExpressionAxiom(std::unique_ptr<cola::Expression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct OwnershipAxiom : public Axiom {
		std::unique_ptr<cola::VariableExpression> expr;

		OwnershipAxiom(std::unique_ptr<cola::VariableExpression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct DataStructureLogicallyContainsAxiom : public Axiom {
		std::unique_ptr<cola::Expression> value;

		DataStructureLogicallyContainsAxiom(std::unique_ptr<cola::Expression> value);
		ACCEPT_FORMULA_VISITOR
	};

	struct NodeLogicallyContainsAxiom : public Axiom {
		std::unique_ptr<cola::Expression> node;
		std::unique_ptr<cola::Expression> value;

		NodeLogicallyContainsAxiom(std::unique_ptr<cola::Expression> node, std::unique_ptr<cola::Expression> value);
		ACCEPT_FORMULA_VISITOR
	};

	struct KeysetContainsAxiom : public Axiom {
		std::unique_ptr<cola::Expression> node;
		std::unique_ptr<cola::Expression> value;

		KeysetContainsAxiom(std::unique_ptr<cola::Expression> node, std::unique_ptr<cola::Expression> value);
		ACCEPT_FORMULA_VISITOR
	};

	struct HasFlowAxiom : public Axiom {
		std::unique_ptr<cola::Expression> expr;

		HasFlowAxiom(std::unique_ptr<cola::Expression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct UniqueInflowAxiom : public Axiom {
		std::unique_ptr<cola::Expression> node;

		UniqueInflowAxiom(std::unique_ptr<cola::Expression> node);
		ACCEPT_FORMULA_VISITOR
	};

	struct FlowContainsAxiom : public Axiom { // TODO: use a 'Predicate' instead of 'value_low' and 'value_high'
		std::unique_ptr<cola::Expression> node;
		std::unique_ptr<cola::Expression> value_low;
		std::unique_ptr<cola::Expression> value_high;

		FlowContainsAxiom(std::unique_ptr<cola::Expression> node, std::unique_ptr<cola::Expression> value_low, std::unique_ptr<cola::Expression> value_high);
		ACCEPT_FORMULA_VISITOR
	};

	struct SpecificationAxiom : public Axiom {
		enum struct Kind { CONTAINS, INSERT, DELETE };
		static constexpr std::array<Kind, 3> KindIteratable { Kind::CONTAINS, Kind::INSERT, Kind::DELETE };
		Kind kind;
		std::unique_ptr<cola::VariableExpression> key;

		SpecificationAxiom(Kind kind, std::unique_ptr<cola::VariableExpression> key);
	};

	inline std::string to_string(SpecificationAxiom::Kind kind) {
		switch (kind) {
			case SpecificationAxiom::Kind::CONTAINS: return "contains";
			case SpecificationAxiom::Kind::INSERT: return "insert";
			case SpecificationAxiom::Kind::DELETE: return "delete";
		}
	}

	struct ObligationAxiom : public SpecificationAxiom {
		ObligationAxiom(Kind kind, std::unique_ptr<cola::VariableExpression> key);
		ACCEPT_FORMULA_VISITOR
	};

	struct FulfillmentAxiom : public SpecificationAxiom {
		bool return_value;

		FulfillmentAxiom(Kind kind, std::unique_ptr<cola::VariableExpression> key, bool return_value);
		ACCEPT_FORMULA_VISITOR
	};

	//
	// Predicates
	//

	struct TimePredicate : public Formula {
	};

	struct PastPredicate : public TimePredicate {
		std::unique_ptr<ConjunctionFormula> formula;

		PastPredicate(std::unique_ptr<ConjunctionFormula> formula);
		ACCEPT_FORMULA_VISITOR
	};

	struct FuturePredicate : public TimePredicate  {
		std::unique_ptr<ConjunctionFormula> pre;
		std::unique_ptr<cola::Assignment> command;
		std::unique_ptr<ConjunctionFormula> post;

		FuturePredicate(std::unique_ptr<ConjunctionFormula> pre, std::unique_ptr<cola::Assignment> cmd, std::unique_ptr<ConjunctionFormula> post);
		ACCEPT_FORMULA_VISITOR
	};

	//
	// Annotation
	//

	struct Annotation : public Formula {
		std::unique_ptr<ConjunctionFormula> now;
		std::deque<std::unique_ptr<TimePredicate>> time;

		Annotation();
		Annotation(std::unique_ptr<ConjunctionFormula> now);
		Annotation(std::unique_ptr<ConjunctionFormula> now, std::deque<std::unique_ptr<TimePredicate>> time);
		static std::unique_ptr<Annotation> make_true();
		static std::unique_ptr<Annotation> make_false();
		ACCEPT_FORMULA_VISITOR
	};

} // namespace heal

#endif