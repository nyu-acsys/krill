#pragma once
#ifndef PLANKTON_LOGICS_AST_HPP
#define PLANKTON_LOGICS_AST_HPP

#include <set>
#include <deque>
#include <memory>
#include "visitors.hpp"
#include "programs/ast.hpp"

namespace plankton {
    
    struct LogicObject {
        explicit LogicObject() = default;
        LogicObject(const LogicObject& other) = delete;
        virtual ~LogicObject() = default;
        
        virtual void Accept(LogicVisitor& visitor) const = 0;
        virtual void Accept(MutableLogicVisitor& visitor) = 0;
    };

	#define ACCEPT_LOGIC_VISITOR \
	    void Accept(LogicVisitor& visitor) const override { visitor.Visit(*this); } \
        void Accept(MutableLogicVisitor& visitor) override { visitor.Visit(*this); }

    //
    // Symbolic Variables
    //
    
    enum struct Order { FIRST, SECOND };

    struct SymbolDeclaration final {
        std::string name;
        const Type& type;
        Order order;
        
        SymbolDeclaration(const SymbolDeclaration&) = delete;
        
        private:
            explicit SymbolDeclaration(std::string name, const Type& type, Order order);
            friend struct SymbolFactory;
    };
    
    struct SymbolFactory final {
        explicit SymbolFactory();
        explicit SymbolFactory(const LogicObject& avoid);
        
        void Avoid(const LogicObject& avoid);
        const SymbolDeclaration& GetFresh(const Type& type, Order order);
        
        private:
            std::set<const SymbolDeclaration*> inUse;
    };

    //
    // Symbolic Expressions
    //

    struct SymbolicExpression : public LogicObject {
        [[nodiscard]] virtual const Type& Type() const = 0;
        [[nodiscard]] Sort Sort() const { return Type().sort; }
    };

    struct SymbolicVariable final : SymbolicExpression {
        std::reference_wrapper<const SymbolDeclaration> decl;

        explicit SymbolicVariable(const SymbolDeclaration& decl);
        [[nodiscard]] const plankton::Type& Type() const override;
        [[nodiscard]] inline const SymbolDeclaration& Decl() const { return decl; };
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicBool final : public SymbolicExpression {
        bool value;

        explicit SymbolicBool(bool value);
        [[nodiscard]] const plankton::Type& Type() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicNull final : public SymbolicExpression {
        const plankton::Type& type;
        
        explicit SymbolicNull(const plankton::Type& type);
        [[nodiscard]] const plankton::Type& Type() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMin final : public SymbolicExpression {
        explicit SymbolicMin();
        [[nodiscard]] const plankton::Type& Type() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMax final : public SymbolicExpression {
        explicit SymbolicMax();
        [[nodiscard]] const plankton::Type& Type() const override;
        ACCEPT_LOGIC_VISITOR
    };

    //
    // Formulas
    //

    struct Formula : public LogicObject {
    };

    struct SeparatingConjunction : public Formula {
        std::deque<std::unique_ptr<Formula>> conjuncts;

        explicit SeparatingConjunction();
        ACCEPT_LOGIC_VISITOR
    };
    
    struct MemoryAxiom : public Formula {
        std::unique_ptr<SymbolicVariable> node;
        std::unique_ptr<SymbolicVariable> flow;
        std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    
        explicit MemoryAxiom(std::unique_ptr<SymbolicVariable> adr, std::unique_ptr<SymbolicVariable> flw,
                             std::map<std::string, std::unique_ptr<SymbolicVariable>> fields);
    };
    
    struct LocalMemoryResource final : public MemoryAxiom {
        using MemoryAxiom::MemoryAxiom;
        ACCEPT_LOGIC_VISITOR
    };
    
    struct SharedMemoryCore final : public MemoryAxiom {
        using MemoryAxiom::MemoryAxiom;
        ACCEPT_LOGIC_VISITOR
    };

    struct EqualsToAxiom final : public Formula {
        std::unique_ptr<VariableExpression> variable;
        std::unique_ptr<SymbolicVariable> value;

        explicit EqualsToAxiom(std::unique_ptr<VariableExpression> variable,
                               std::unique_ptr<SymbolicVariable> value);
        ACCEPT_LOGIC_VISITOR
    };

    struct StackAxiom final : public Formula {
        BinaryOperator op;
        std::unique_ptr<SymbolicExpression> lhs;
        std::unique_ptr<SymbolicExpression> rhs;
    
        explicit StackAxiom(BinaryOperator op, std::unique_ptr<SymbolicExpression> lhs,
                            std::unique_ptr<SymbolicExpression> rhs);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowEmptinessAxiom final : public Formula {
        std::unique_ptr<SymbolicVariable> flow;
        bool isEmpty;

        explicit InflowEmptinessAxiom(std::unique_ptr<SymbolicVariable> flow, bool isEmpty);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowContainsValueAxiom final : public Formula {
        std::unique_ptr<SymbolicVariable> flow;
        std::unique_ptr<SymbolicVariable> value;
    
        explicit InflowContainsValueAxiom(std::unique_ptr<SymbolicVariable> flow,
                                          std::unique_ptr<SymbolicVariable> value);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowContainsRangeAxiom final : public Formula {
        std::unique_ptr<SymbolicVariable> flow;
        std::unique_ptr<SymbolicExpression> valueLow; // included in range
        std::unique_ptr<SymbolicExpression> valueHigh; // included in range

        explicit InflowContainsRangeAxiom(std::unique_ptr<SymbolicVariable> flow,
                                          std::unique_ptr<SymbolicExpression> valueLow,
                                          std::unique_ptr<SymbolicExpression> valueHigh);
        ACCEPT_LOGIC_VISITOR
    };

    struct ObligationAxiom final : public Formula {
        enum struct Kind {
            CONTAINS, INSERT, DELETE
        };
        
        Kind kind;
        std::unique_ptr<SymbolicVariable> key;

        explicit ObligationAxiom(Kind kind, std::unique_ptr<SymbolicVariable> key);
        ACCEPT_LOGIC_VISITOR
    };

    struct FulfillmentAxiom final : public Formula {
        bool returnValue;

        explicit FulfillmentAxiom(bool returnValue);
        ACCEPT_LOGIC_VISITOR
    };

    //
    // Implications
    //
    
    struct SeparatingImplication : public LogicObject {
        std::unique_ptr<Formula> premise;
        std::unique_ptr<Formula> conclusion;

        explicit SeparatingImplication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion);
        ACCEPT_LOGIC_VISITOR
    };

    //
    // Predicates
    //

    struct PastPredicate final : public LogicObject {
        std::unique_ptr<Formula> formula; // TODO: SeparatingConjunction?

        explicit PastPredicate(std::unique_ptr<Formula> formula);
        ACCEPT_LOGIC_VISITOR
    };

    struct FuturePredicate final : public LogicObject  {
        const MemoryWrite& command;
        std::unique_ptr<Formula> pre;  // TODO: SeparatingConjunction?
        std::unique_ptr<Formula> post;  // TODO: SeparatingConjunction?

        explicit FuturePredicate(const MemoryWrite& command, std::unique_ptr<Formula> pre,
                                 std::unique_ptr<Formula> post);
        ACCEPT_LOGIC_VISITOR
    };

    //
    // Annotation
    //

    struct Annotation final : public LogicObject {
        std::unique_ptr<SeparatingConjunction> now;
        std::deque<std::unique_ptr<PastPredicate>> past;
        std::deque<std::unique_ptr<FuturePredicate>> future;

        explicit Annotation();
        explicit Annotation(std::unique_ptr<SeparatingConjunction> now);
        explicit Annotation(std::unique_ptr<SeparatingConjunction> now,
                            std::deque<std::unique_ptr<PastPredicate>> past,
                            std::deque<std::unique_ptr<FuturePredicate>> future);
        ACCEPT_LOGIC_VISITOR
    };

    //
    // Output
    //

    std::ostream& operator<<(std::ostream& out, const LogicObject& object);
    std::ostream& operator<<(std::ostream& out, const SymbolDeclaration& object);
    std::ostream& operator<<(std::ostream& out, ObligationAxiom::Kind object);

} // namespace plankton

#endif //PLANKTON_LOGICS_AST_HPP
