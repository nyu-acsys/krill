#pragma once
#ifndef HEAL_LOGIC
#define HEAL_LOGIC


#include <array>
#include <deque>
#include <memory>
#include "cola/ast.hpp"
#include "visitors.hpp"


namespace heal {

	//
	// Formulas
	//

	struct LogicObject {
        LogicObject(const LogicObject& other) = delete;
		virtual ~LogicObject() = default;
		virtual void accept(LogicVisitor& visitor) const = 0;
		virtual void accept(LogicNonConstVisitor& visitor) = 0;
		protected: LogicObject() = default;
	};

	#define ACCEPT_FORMULA_VISITOR \
		virtual void accept(LogicNonConstVisitor& visitor) override { visitor.visit(*this); } \
		virtual void accept(LogicVisitor& visitor) const override { visitor.visit(*this); }

	//
	// Formulas
	//

    struct Formula : public LogicObject {
    };

	struct ImplicationFormula : public Formula {
		std::unique_ptr<Formula> premise;
		std::unique_ptr<Formula> conclusion;

        ImplicationFormula();
        ImplicationFormula(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion);
		ACCEPT_FORMULA_VISITOR
	};

	struct ConjunctionFormula : public Formula {
		std::deque<std::unique_ptr<Formula>> conjuncts;

        ConjunctionFormula();
        explicit ConjunctionFormula(std::deque<std::unique_ptr<Formula>> conjuncts);

		decltype(conjuncts)::iterator begin() { return conjuncts.begin(); }
		decltype(conjuncts)::iterator end() { return conjuncts.end(); }
		[[nodiscard]] decltype(conjuncts)::const_iterator begin() const { return conjuncts.cbegin(); }
		[[nodiscard]] decltype(conjuncts)::const_iterator end() const { return conjuncts.cend(); }
		ACCEPT_FORMULA_VISITOR
	};

    struct NegationFormula : public Formula {
        std::unique_ptr<Formula> formula;

        explicit NegationFormula(std::unique_ptr<Formula> formula);
        ACCEPT_FORMULA_VISITOR
    };

	//
	// Axioms
	//

    struct Axiom : public Formula {
    };

	struct ExpressionAxiom : public Axiom {
		std::unique_ptr<cola::Expression> expr;

		explicit ExpressionAxiom(std::unique_ptr<cola::Expression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct OwnershipAxiom : public Axiom {
		std::unique_ptr<cola::VariableExpression> expr;

		explicit OwnershipAxiom(std::unique_ptr<cola::VariableExpression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct DataStructureLogicallyContainsAxiom : public Axiom {
		std::unique_ptr<cola::Expression> value;

		explicit DataStructureLogicallyContainsAxiom(std::unique_ptr<cola::Expression> value);
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

		explicit HasFlowAxiom(std::unique_ptr<cola::Expression> expr);
		ACCEPT_FORMULA_VISITOR
	};

	struct UniqueInflowAxiom : public Axiom {
		std::unique_ptr<cola::Expression> node;

		explicit UniqueInflowAxiom(std::unique_ptr<cola::Expression> node);
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
		static constexpr std::array<Kind, 3> KindIterable {Kind::CONTAINS, Kind::INSERT, Kind::DELETE };
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

	struct TimePredicate : public LogicObject {
	};

	struct PastPredicate : public TimePredicate {
		std::unique_ptr<Formula> formula;

		explicit PastPredicate(std::unique_ptr<Formula> formula);
		ACCEPT_FORMULA_VISITOR
	};

	struct FuturePredicate : public TimePredicate  {
		std::unique_ptr<Formula> pre;
		std::unique_ptr<cola::Assignment> command;
		std::unique_ptr<Formula> post;

		FuturePredicate(std::unique_ptr<Formula> pre, std::unique_ptr<cola::Assignment> cmd, std::unique_ptr<Formula> post);
		ACCEPT_FORMULA_VISITOR
	};

	//
	// Annotation
	//

	struct Annotation : public LogicObject {
		std::unique_ptr<Formula> now;
		std::deque<std::unique_ptr<TimePredicate>> time;

		Annotation();
        Annotation(std::unique_ptr<Formula> now);
        Annotation(std::unique_ptr<Formula> now, std::deque<std::unique_ptr<TimePredicate>> time);
		static std::unique_ptr<Annotation> True();
		static std::unique_ptr<Annotation> False();
		ACCEPT_FORMULA_VISITOR
	};

} // namespace heal

#endif