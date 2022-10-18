#pragma once
#ifndef PLANKTON_LOGICS_AST_HPP
#define PLANKTON_LOGICS_AST_HPP

#include <set>
#include <deque>
#include <memory>
#include <functional>
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
        
        [[nodiscard]] bool operator==(const SymbolDeclaration& other) const;
        [[nodiscard]] bool operator!=(const SymbolDeclaration& other) const;
        
        private:
            explicit SymbolDeclaration(std::string name, const Type& type, Order order);
            friend struct SymbolFactory;
    };
    
    struct SymbolFactory final {
        explicit SymbolFactory();
        explicit SymbolFactory(const LogicObject& avoid);
        SymbolFactory(SymbolFactory&& other) = default;
        SymbolFactory(const SymbolFactory& other) = delete;

        void Avoid(const LogicObject& avoid);
        void Avoid(const SymbolDeclaration& avoid);
        const SymbolDeclaration& GetFresh(const Type& type, Order order);
        inline const SymbolDeclaration& GetFreshFO(const Type& type) { return GetFresh(type, Order::FIRST); }
        inline const SymbolDeclaration& GetFreshSO(const Type& type) { return GetFresh(type, Order::SECOND); }
        
        private:
            std::set<const SymbolDeclaration*> inUse;
    };

    //
    // Symbolic Expressions
    //

    struct SymbolicExpression : public LogicObject {
        [[nodiscard]] virtual Order GetOrder() const = 0;
        [[nodiscard]] virtual const Type& GetType() const = 0;
        [[nodiscard]] Sort GetSort() const { return GetType().sort; }
    };

    struct SymbolicVariable final : SymbolicExpression {
        std::reference_wrapper<const SymbolDeclaration> decl;

        explicit SymbolicVariable(const SymbolDeclaration& decl);
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
        [[nodiscard]] inline const SymbolDeclaration& Decl() const { return decl; };
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicBool final : public SymbolicExpression {
        bool value;

        explicit SymbolicBool(bool value);
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicNull final : public SymbolicExpression {
        explicit SymbolicNull();
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMin final : public SymbolicExpression {
        explicit SymbolicMin();
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicMax final : public SymbolicExpression {
        explicit SymbolicMax();
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicSelfTid final : public  SymbolicExpression {
        explicit SymbolicSelfTid();
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicSomeTid final : public  SymbolicExpression {
        explicit SymbolicSomeTid();
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
        ACCEPT_LOGIC_VISITOR
    };

    struct SymbolicUnlocked final : public  SymbolicExpression {
        explicit SymbolicUnlocked();
        [[nodiscard]] Order GetOrder() const override;
        [[nodiscard]] const Type& GetType() const override;
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
        void Conjoin(std::unique_ptr<Formula> formula);
        void RemoveConjunctsIf(const std::function<bool(const Formula&)>& unaryPredicate);
        ACCEPT_LOGIC_VISITOR
    };
    
    struct MemoryAxiom : public Axiom {
        std::unique_ptr<SymbolicVariable> node;
        std::unique_ptr<SymbolicVariable> flow;
        std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    
        explicit MemoryAxiom(const SymbolDeclaration& node, const SymbolDeclaration& flow,
                             const std::map<std::string, std::reference_wrapper<const SymbolDeclaration>>& fieldToValue);
        explicit MemoryAxiom(std::unique_ptr<SymbolicVariable> node, std::unique_ptr<SymbolicVariable> flow,
                             std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue);
    };
    
    struct LocalMemoryResource final : public MemoryAxiom {
        using MemoryAxiom::MemoryAxiom;
        ACCEPT_LOGIC_VISITOR
    };
    
    struct SharedMemoryCore final : public MemoryAxiom {
        using MemoryAxiom::MemoryAxiom;
        ACCEPT_LOGIC_VISITOR
    };

    struct EqualsToAxiom final : public Axiom {
        std::reference_wrapper<const VariableDeclaration> variable;
        std::unique_ptr<SymbolicVariable> value;

        explicit EqualsToAxiom(const VariableDeclaration& variable, const SymbolDeclaration& value);
        explicit EqualsToAxiom(const VariableDeclaration& variable, std::unique_ptr<SymbolicVariable> value);
        [[nodiscard]] inline const VariableDeclaration& Variable() const { return variable; }
        [[nodiscard]] inline const SymbolDeclaration& Value() const { return value->Decl(); }
        ACCEPT_LOGIC_VISITOR
    };

    struct StackAxiom final : public Axiom {
        BinaryOperator op;
        std::unique_ptr<SymbolicExpression> lhs;
        std::unique_ptr<SymbolicExpression> rhs;
    
        explicit StackAxiom(BinaryOperator op, std::unique_ptr<SymbolicExpression> lhs,
                            std::unique_ptr<SymbolicExpression> rhs);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowEmptinessAxiom final : public Axiom {
        std::unique_ptr<SymbolicVariable> flow;
        bool isEmpty;

        explicit InflowEmptinessAxiom(const SymbolDeclaration& flow, bool isEmpty);
        explicit InflowEmptinessAxiom(std::unique_ptr<SymbolicVariable> flow, bool isEmpty);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowContainsValueAxiom final : public Axiom {
        std::unique_ptr<SymbolicVariable> flow;
        std::unique_ptr<SymbolicVariable> value;
    
        explicit InflowContainsValueAxiom(const SymbolDeclaration& flow, const SymbolDeclaration& value);
        explicit InflowContainsValueAxiom(std::unique_ptr<SymbolicVariable> flow,
                                          std::unique_ptr<SymbolicVariable> value);
        ACCEPT_LOGIC_VISITOR
    };

    struct InflowContainsRangeAxiom final : public Axiom {
        std::unique_ptr<SymbolicVariable> flow;
        std::unique_ptr<SymbolicExpression> valueLow; // included in range
        std::unique_ptr<SymbolicExpression> valueHigh; // included in range

        explicit InflowContainsRangeAxiom(const SymbolDeclaration& flow,
                                          const SymbolDeclaration& valueLow, const SymbolDeclaration& valueHigh);
        explicit InflowContainsRangeAxiom(std::unique_ptr<SymbolicVariable> flow,
                                          std::unique_ptr<SymbolicExpression> valueLow,
                                          std::unique_ptr<SymbolicExpression> valueHigh);
        ACCEPT_LOGIC_VISITOR
    };
    
    enum struct Specification {
        CONTAINS, INSERT, DELETE
    };

    struct ObligationAxiom final : public Axiom {
        Specification spec;
        std::unique_ptr<SymbolicVariable> key;

        explicit ObligationAxiom(Specification spec, const SymbolDeclaration& key);
        explicit ObligationAxiom(Specification spec, std::unique_ptr<SymbolicVariable> key);
        ACCEPT_LOGIC_VISITOR
    };

    struct FulfillmentAxiom final : public Axiom {
        bool returnValue;

        explicit FulfillmentAxiom(bool returnValue);
        ACCEPT_LOGIC_VISITOR
    };

    //
    // Invariants
    //
    
    struct NonSeparatingImplication : public LogicObject { // may not handle resources in an SL way
        std::unique_ptr<SeparatingConjunction> premise;
        std::unique_ptr<SeparatingConjunction> conclusion;

        explicit NonSeparatingImplication();
        explicit NonSeparatingImplication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion);
        ACCEPT_LOGIC_VISITOR
    };
    
    struct ImplicationSet : public LogicObject {
        std::deque<std::unique_ptr<NonSeparatingImplication>> conjuncts;
        
        explicit ImplicationSet();
        void Conjoin(std::unique_ptr<NonSeparatingImplication> implication);
        ACCEPT_LOGIC_VISITOR
    };
    
    //
    // Predicates
    //

    struct PastPredicate final : public LogicObject {
        std::unique_ptr<SharedMemoryCore> formula;

        explicit PastPredicate(std::unique_ptr<SharedMemoryCore> formula);
        ACCEPT_LOGIC_VISITOR
    };

    struct Guard : public LogicObject {
        std::deque<std::unique_ptr<BinaryExpression>> conjuncts;

        explicit Guard();
        explicit Guard(std::unique_ptr<BinaryExpression> expression);
        explicit Guard(std::deque<std::unique_ptr<BinaryExpression>> conjuncts);
        ACCEPT_LOGIC_VISITOR
    };

    struct Update : public LogicObject {
        std::vector<std::unique_ptr<Dereference>> fields;
        std::vector<std::unique_ptr<SymbolicExpression>> values;

        explicit Update();
        explicit Update(std::unique_ptr<Dereference> field, std::unique_ptr<SymbolicExpression> value);
        explicit Update(std::vector<std::unique_ptr<Dereference>> fields, std::vector<std::unique_ptr<SymbolicExpression>> values);
        ACCEPT_LOGIC_VISITOR
    };

    struct FuturePredicate final : public LogicObject  {
        std::unique_ptr<Guard> guard;
        std::unique_ptr<Update> update;

        explicit FuturePredicate(std::unique_ptr<Update> update, std::unique_ptr<Guard> guard);
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
        void Conjoin(std::unique_ptr<Formula> formula);
        void Conjoin(std::unique_ptr<PastPredicate> predicate);
        void Conjoin(std::unique_ptr<FuturePredicate> predicate);
        ACCEPT_LOGIC_VISITOR
    };

    //
    // Output
    //

    std::ostream& operator<<(std::ostream& out, const LogicObject& object);
    std::ostream& operator<<(std::ostream& out, const SymbolDeclaration& object);
    std::ostream& operator<<(std::ostream& out, Specification object);

} // namespace plankton

#endif //PLANKTON_LOGICS_AST_HPP
