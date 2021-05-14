#pragma once
#ifndef HEAL_LOGIC
#define HEAL_LOGIC

#include <iostream> // TODO: remove
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
            friend struct SymbolicPool;
    };

    struct SymbolicFlowDeclaration final : public cola::VariableDeclaration {
        SymbolicFlowDeclaration(const SymbolicFlowDeclaration&) = delete;

        private:
            using cola::VariableDeclaration::VariableDeclaration;
            friend struct SymbolicPool;
    };

    struct SymbolicPool {
        static const SymbolicVariableDeclaration& MakeFreshVariable(const cola::Type& type);
        static const SymbolicFlowDeclaration& MakeFreshFlow(const cola::Type& type);
        static const std::deque<std::unique_ptr<SymbolicVariableDeclaration>>& GetAllVariables();
        static const std::deque<std::unique_ptr<SymbolicFlowDeclaration>>& GetAllFlows();
    };

    //
    // Symbolic Expressions
    //

    struct SymbolicExpression : public LogicObject {
        [[nodiscard]] virtual const cola::Type& Type() const = 0;
        [[nodiscard]] cola::Sort Sort() const { return Type().sort; }
    };

    struct SymbolicVariable : SymbolicExpression {
        std::reference_wrapper<const SymbolicVariableDeclaration> decl_storage;

        explicit SymbolicVariable(const SymbolicVariableDeclaration& decl);
        [[nodiscard]] inline const SymbolicVariableDeclaration& Decl() const { return decl_storage.get(); };
        [[nodiscard]] const cola::Type& Type() const override { return Decl().type; }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicBool : public SymbolicExpression {
        bool value;

        explicit SymbolicBool(bool value) : value(value) {}
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::bool_type(); }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicNull : public SymbolicExpression {
        explicit SymbolicNull() = default;
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::null_type(); }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMin : public SymbolicExpression {
        explicit SymbolicMin() = default;
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::data_type(); }
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMax : public SymbolicExpression {
        explicit SymbolicMax() = default;
        [[nodiscard]] const cola::Type& Type() const override { return cola::Type::data_type(); }
        ACCEPT_LOGIC_VISITOR
    };

	//
	// Formulas
	//

    struct Formula : public LogicObject {
    };

    struct Axiom : public Formula {
    };

    struct SeparatingConjunction : public Formula {
        std::deque<std::unique_ptr<Formula>> conjuncts;

        explicit SeparatingConjunction();
        explicit SeparatingConjunction(std::deque<std::unique_ptr<Formula>> conjuncts);
        ACCEPT_LOGIC_VISITOR
    };

    struct SeparatingImplication : public Formula {
        std::unique_ptr<Formula> premise;
        std::unique_ptr<Formula> conclusion;

        explicit SeparatingImplication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion);
        ACCEPT_LOGIC_VISITOR
    };

//    struct FlatSeparatingConjunction : public Formula {
//        std::deque<std::unique_ptr<Axiom>> conjuncts;
//
//        explicit FlatSeparatingConjunction();
//        explicit FlatSeparatingConjunction(std::deque<std::unique_ptr<Axiom>> conjuncts);
//        ACCEPT_LOGIC_VISITOR
//    };
//
//    struct SeparatingImplication : public Formula {
//        std::unique_ptr<FlatSeparatingConjunction> premise;
//        std::unique_ptr<FlatSeparatingConjunction> conclusion;
//
//        explicit SeparatingImplication(std::unique_ptr<FlatSeparatingConjunction> premise,
//                                       std::unique_ptr<FlatSeparatingConjunction> conclusion);
//        ACCEPT_LOGIC_VISITOR
//    };

	//
	// Axioms
	//

    struct PointsToAxiom : public Axiom {
        std::unique_ptr<SymbolicVariable> node;
        bool isLocal;
        std::reference_wrapper<const SymbolicFlowDeclaration> flow;
        std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;

        explicit PointsToAxiom(std::unique_ptr<SymbolicVariable> node,
                               bool isLocal, const SymbolicFlowDeclaration& flow,
                               std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue);
        ACCEPT_LOGIC_VISITOR
    };

    struct EqualsToAxiom : public Axiom {
        std::unique_ptr<cola::VariableExpression> variable;
        std::unique_ptr<SymbolicVariable> value;

        explicit EqualsToAxiom(std::unique_ptr<cola::VariableExpression> variable,
                               std::unique_ptr<SymbolicVariable> value);
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicAxiom : public Axiom {
        enum Operator { EQ, NEQ, LEQ, LT, GEQ, GT };
        std::unique_ptr<SymbolicExpression> lhs;
        Operator op;
        std::unique_ptr<SymbolicExpression> rhs;

        explicit SymbolicAxiom(std::unique_ptr<SymbolicExpression> lhs, Operator op,
                               std::unique_ptr<SymbolicExpression> rhs);
        ACCEPT_LOGIC_VISITOR
    };

    struct StackDisjunction : public Axiom {
        std::deque<std::unique_ptr<SymbolicAxiom>> axioms;

        explicit StackDisjunction();
        explicit StackDisjunction(std::deque<std::unique_ptr<SymbolicAxiom>> axioms);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowContainsValueAxiom : public Axiom {
        std::reference_wrapper<const SymbolicFlowDeclaration> flow;
        std::unique_ptr<SymbolicVariable> value;

        explicit InflowContainsValueAxiom(const SymbolicFlowDeclaration& flow,
                                          std::unique_ptr<SymbolicVariable> value);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowContainsRangeAxiom : public Axiom {
        std::reference_wrapper<const SymbolicFlowDeclaration> flow;
        std::unique_ptr<SymbolicVariable> valueLow; // not included in range
        std::unique_ptr<SymbolicVariable> valueHigh; // included in range

        explicit InflowContainsRangeAxiom(const SymbolicFlowDeclaration& flow,
                                          std::unique_ptr<SymbolicVariable> valueLow,
                                          std::unique_ptr<SymbolicVariable> valueHigh);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowEmptinessAxiom : public Axiom {
        std::reference_wrapper<const SymbolicFlowDeclaration> flow;
        bool isEmpty;

        explicit InflowEmptinessAxiom(const SymbolicFlowDeclaration& flow, bool isEmpty);
        ACCEPT_LOGIC_VISITOR
    };

	struct SpecificationAxiom : public Axiom {
		enum struct Kind { CONTAINS, INSERT, DELETE };
//		static constexpr std::array<Kind, 3> AllKinds {Kind::CONTAINS, Kind::INSERT, Kind::DELETE };
		Kind kind;
		std::unique_ptr<SymbolicVariable> key;

        explicit SpecificationAxiom(Kind kind, std::unique_ptr<SymbolicVariable> key);
	};

	struct ObligationAxiom : public SpecificationAxiom {
        explicit ObligationAxiom(Kind kind, std::unique_ptr<SymbolicVariable> key);
		ACCEPT_LOGIC_VISITOR
	};

	struct FulfillmentAxiom : public SpecificationAxiom {
		bool return_value;

        explicit FulfillmentAxiom(Kind kind, std::unique_ptr<SymbolicVariable> key, bool return_value);
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
		const cola::Assignment& command;
		std::unique_ptr<Formula> post;

        explicit FuturePredicate(std::unique_ptr<Formula> pre, const cola::Assignment& command,
                                 std::unique_ptr<Formula> post);
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
        explicit Annotation(std::unique_ptr<SeparatingConjunction> now,
                            std::deque<std::unique_ptr<TimePredicate>> time);
		ACCEPT_LOGIC_VISITOR
	};

} // namespace heal

#endif