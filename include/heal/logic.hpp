#pragma once
#ifndef HEAL_LOGIC
#define HEAL_LOGIC


#include <array>
#include <deque>
#include <memory>
#include "cola/ast.hpp"
#include "visitors.hpp"


namespace heal {

	struct LogicObject {
        LogicObject(const LogicObject& other) = delete;
		virtual ~LogicObject() = default;
		virtual void accept(LogicVisitor& visitor) const = 0;
		virtual void accept(LogicNonConstVisitor& visitor) = 0;
		protected: LogicObject() = default;
	};

	#define ACCEPT_LOGIC_VISITOR \
		virtual void accept(LogicNonConstVisitor& visitor) override { visitor.visit(*this); } \
		virtual void accept(LogicVisitor& visitor) const override { visitor.visit(*this); }

    //
    // Symbolic Variables
    //

    struct SymbolicVariableDeclaration final : public cola::VariableDeclaration {
        SymbolicVariableDeclaration(const SymbolicVariableDeclaration&) = delete;

        private:
            using cola::VariableDeclaration::VariableDeclaration;
            friend struct SymbolicVariablePool;
    };

    struct SymbolicVariablePool {
        static const SymbolicVariableDeclaration& MakeFresh(const cola::Type& type);
        static const std::deque<std::unique_ptr<SymbolicVariableDeclaration>>& GetAll();
    };

    //
    // Expressions
    //

    struct StackExpression : public LogicObject {
        [[nodiscard]] virtual const cola::Type& Type() const = 0;
        [[nodiscard]] cola::Sort Sort() const { return Type().sort; }
    };

    struct LogicVariable : StackExpression {
        std::reference_wrapper<const cola::VariableDeclaration> decl_storage;

        explicit LogicVariable(const cola::VariableDeclaration& decl);
        [[nodiscard]] inline const cola::VariableDeclaration& Decl() const { return decl_storage.get(); };
        [[nodiscard]] const cola::Type& Type() const override { return Decl().type; }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicBool : public StackExpression {
        bool value;

        explicit SymbolicBool(bool value) : value(value) {}
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::bool_type(); }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicNull : public StackExpression {
        explicit SymbolicNull() = default;
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::null_type(); }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMin : public StackExpression {
        explicit SymbolicMin() = default;
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::data_type(); }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMax : public StackExpression {
        explicit SymbolicMax() = default;
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::data_type(); }
        ACCEPT_LOGIC_VISITOR
    };

	//
	// Formulas
	//

    struct Formula : public LogicObject {
    };

    struct FlatFormula : public Formula {
    };

    struct Axiom : public FlatFormula {
    };

    struct SeparatingConjunction : public Formula {
        std::deque<std::unique_ptr<Formula>> conjuncts;

        explicit SeparatingConjunction();
        explicit SeparatingConjunction(std::deque<std::unique_ptr<Formula>> conjuncts);
        ACCEPT_LOGIC_VISITOR
    };

    struct FlatSeparatingConjunction : public FlatFormula {
        std::deque<std::unique_ptr<Axiom>> conjuncts;

        explicit FlatSeparatingConjunction();
        explicit FlatSeparatingConjunction(std::deque<std::unique_ptr<Axiom>> conjuncts);
        ACCEPT_LOGIC_VISITOR
    };

    struct SeparatingImplication : public Formula {
        std::unique_ptr<FlatFormula> premise;
        std::unique_ptr<FlatFormula> conclusion;

        explicit SeparatingImplication(std::unique_ptr<FlatFormula> premise, std::unique_ptr<FlatFormula> conclusion);
        ACCEPT_LOGIC_VISITOR
    };

	//
	// Axioms
	//

    struct NegatedAxiom : public Axiom {
        std::unique_ptr<Axiom> axiom;

        explicit NegatedAxiom(std::unique_ptr<Axiom> axiom);
        ACCEPT_LOGIC_VISITOR
    };

    struct BoolAxiom : public Axiom {
        bool value;

        explicit BoolAxiom(bool value) : value(value) {}
        ACCEPT_LOGIC_VISITOR
    };

    struct PointsToAxiom : public Axiom {
        std::unique_ptr<LogicVariable> node;
        std::string fieldname;
        std::unique_ptr<LogicVariable> value;

        explicit PointsToAxiom(std::unique_ptr<LogicVariable> node, std::string fieldname, std::unique_ptr<LogicVariable> value);
        ACCEPT_LOGIC_VISITOR
    };

    struct StackAxiom : public Axiom {
        enum Operator { EQ, NEQ, LEQ, LT, GEQ, GT };
        std::unique_ptr<StackExpression> lhs;
        Operator op;
        std::unique_ptr<StackExpression> rhs;

        explicit StackAxiom(std::unique_ptr<StackExpression> lhs, Operator op, std::unique_ptr<StackExpression> rhs);
        ACCEPT_LOGIC_VISITOR
    };

    struct StackDisjunction : public Axiom {
        std::deque<std::unique_ptr<StackAxiom>> axioms;

        StackDisjunction();
        explicit StackDisjunction(std::deque<std::unique_ptr<StackAxiom>> axioms);
        ACCEPT_LOGIC_VISITOR
    };

	struct OwnershipAxiom : public Axiom {
        std::unique_ptr<LogicVariable> node;

		explicit OwnershipAxiom(std::unique_ptr<LogicVariable> node);
		ACCEPT_LOGIC_VISITOR
	};

	struct DataStructureLogicallyContainsAxiom : public Axiom {
        std::unique_ptr<LogicVariable> value;

		explicit DataStructureLogicallyContainsAxiom(std::unique_ptr<LogicVariable> value);
		ACCEPT_LOGIC_VISITOR
	};

	struct NodeLogicallyContainsAxiom : public Axiom {
        std::unique_ptr<LogicVariable> node;
        std::unique_ptr<LogicVariable> value;

        explicit NodeLogicallyContainsAxiom(std::unique_ptr<LogicVariable> node, std::unique_ptr<LogicVariable> value);
		ACCEPT_LOGIC_VISITOR
	};

	struct KeysetContainsAxiom : public Axiom {
        std::unique_ptr<LogicVariable> node;
        std::unique_ptr<LogicVariable> value;

        explicit KeysetContainsAxiom(std::unique_ptr<LogicVariable> node, std::unique_ptr<LogicVariable> value);
		ACCEPT_LOGIC_VISITOR
	};

	struct HasFlowAxiom : public Axiom {
        std::unique_ptr<LogicVariable> node;

		explicit HasFlowAxiom(std::unique_ptr<LogicVariable> node);
		ACCEPT_LOGIC_VISITOR
	};

	struct UniqueInflowAxiom : public Axiom {
        std::unique_ptr<LogicVariable> node;

		explicit UniqueInflowAxiom(std::unique_ptr<LogicVariable> node);
		ACCEPT_LOGIC_VISITOR
	};

	struct FlowContainsAxiom : public Axiom { // TODO: use a 'Predicate' instead of 'value_low' and 'value_high'
        std::unique_ptr<LogicVariable> node;
        std::unique_ptr<StackExpression> value_low; // TODO: StackExpression vs SymbolicVariable
        std::unique_ptr<StackExpression> value_high; // TODO: StackExpression vs SymbolicVariable

        explicit FlowContainsAxiom(std::unique_ptr<LogicVariable> node, std::unique_ptr<StackExpression> value_low, std::unique_ptr<StackExpression> value_high);
		ACCEPT_LOGIC_VISITOR
	};

	struct SpecificationAxiom : public Axiom {
		enum struct Kind { CONTAINS, INSERT, DELETE };
		static constexpr std::array<Kind, 3> KindIterable {Kind::CONTAINS, Kind::INSERT, Kind::DELETE };
		Kind kind;
		std::unique_ptr<LogicVariable> key;

        explicit SpecificationAxiom(Kind kind, std::unique_ptr<LogicVariable> key);
	};

	struct ObligationAxiom : public SpecificationAxiom {
        explicit ObligationAxiom(Kind kind, std::unique_ptr<LogicVariable> key);
		ACCEPT_LOGIC_VISITOR
	};

	struct FulfillmentAxiom : public SpecificationAxiom {
		bool return_value;

        explicit FulfillmentAxiom(Kind kind, std::unique_ptr<LogicVariable> key, bool return_value);
		ACCEPT_LOGIC_VISITOR
	};

	//
	// Predicates
	//

	struct TimePredicate : public LogicObject {
	};

	struct PastPredicate : public TimePredicate {
		std::unique_ptr<Formula> formula;

		explicit PastPredicate(std::unique_ptr<Formula> formula);
		ACCEPT_LOGIC_VISITOR
	};

	struct FuturePredicate : public TimePredicate  {
		std::unique_ptr<Formula> pre;
		std::unique_ptr<cola::Assignment> command;
		std::unique_ptr<Formula> post;

        explicit FuturePredicate(std::unique_ptr<Formula> pre, std::unique_ptr<cola::Assignment> cmd, std::unique_ptr<Formula> post);
		ACCEPT_LOGIC_VISITOR
	};

	//
	// Annotation
	//

	struct Annotation : public LogicObject {
		std::unique_ptr<SeparatingConjunction> now;
		std::deque<std::unique_ptr<TimePredicate>> time;

        explicit Annotation();
        explicit Annotation(std::unique_ptr<SeparatingConjunction> now);
        explicit Annotation(std::unique_ptr<SeparatingConjunction> now, std::deque<std::unique_ptr<TimePredicate>> time);
		static std::unique_ptr<Annotation> True();
		static std::unique_ptr<Annotation> False();
		ACCEPT_LOGIC_VISITOR
	};

} // namespace heal

#endif