#pragma once
#ifndef SOLVER_COLLECT
#define SOLVER_COLLECT

#include <set>
#include "heal/logic.hpp"

namespace heal {

    struct BaseResourceVisitor : public heal::DefaultLogicListener {
        void visit(const heal::StackDisjunction& axiom) override { if(axiom.axioms.size() == 1) axiom.axioms.front()->accept(*this); }
        void visit(const heal::SeparatingImplication&) override { /* do nothing */ }
        void visit(const heal::PastPredicate&) override { /* do nothing */ }
        void visit(const heal::FuturePredicate&) override { /* do nothing */ }
    };

    struct BaseNonConstResourceVisitor : public heal::DefaultLogicNonConstListener {
        void visit(heal::StackDisjunction& axiom) override { if(axiom.axioms.size() == 1) axiom.axioms.front()->accept(*this); }
        void visit(heal::SeparatingImplication&) override { /* do nothing */ }
        void visit(heal::PastPredicate&) override { /* do nothing */ }
        void visit(heal::FuturePredicate&) override { /* do nothing */ }
    };

    static std::set<const heal::SymbolicVariableDeclaration*> CollectMemoryNodes(const heal::LogicObject& object) {
        struct Collector : public BaseResourceVisitor {
            std::set<const heal::SymbolicVariableDeclaration*> result;
            void enter(const heal::PointsToAxiom& obj) override { result.insert(&obj.node->Decl()); }
        } collector;
        object.accept(collector);
        return std::move(collector.result);
    }

    static std::set<const heal::PointsToAxiom*> CollectMemory(const heal::LogicObject& object) {
        struct Collector : public BaseResourceVisitor {
            std::set<const heal::PointsToAxiom*> result;
            void enter(const heal::PointsToAxiom& obj) override { result.insert(&obj); }
        } collector;
        object.accept(collector);
        return std::move(collector.result);
    }

    static std::set<const heal::EqualsToAxiom*> CollectVariables(const heal::LogicObject& object) {
        struct Collector : public BaseResourceVisitor {
            std::set<const heal::EqualsToAxiom*> result;
            void enter(const heal::EqualsToAxiom& obj) override { result.insert(&obj); }
        } collector;
        object.accept(collector);
        return std::move(collector.result);
    }

    static std::set<const cola::VariableDeclaration*> CollectVariableNames(const heal::LogicObject& object) {
        struct Collector : public BaseResourceVisitor {
            std::set<const cola::VariableDeclaration*> result;
            void enter(const heal::EqualsToAxiom& obj) override { result.insert(&obj.variable->decl); }
        } collector;
        object.accept(collector);
        return std::move(collector.result);
    }

    static std::set<const heal::ObligationAxiom*> CollectObligations(const heal::LogicObject& object) {
        struct Collector : public BaseResourceVisitor {
            std::set<const heal::ObligationAxiom*> result;
            void enter(const heal::ObligationAxiom& obj) override { result.insert(&obj); }
        } collector;
        object.accept(collector);
        return std::move(collector.result);
    }

    static std::set<const heal::FulfillmentAxiom*> CollectFulfillments(const heal::LogicObject& object) {
        struct Collector : public BaseResourceVisitor {
            std::set<const heal::FulfillmentAxiom*> result;
            void enter(const heal::FulfillmentAxiom& obj) override { result.insert(&obj); }
        } collector;
        object.accept(collector);
        return std::move(collector.result);
    }

    static bool ContainsResources(const heal::LogicObject& object) {
        struct Checker : public DefaultLogicListener {
            bool result = false;
            void enter(const heal::PointsToAxiom& obj) override { result = true; }
            void enter(const heal::EqualsToAxiom& obj) override { result = true; }
            void enter(const heal::ObligationAxiom& obj) override { result = true; }
            void enter(const heal::FulfillmentAxiom& obj) override { result = true; }
        } checker;
        object.accept(checker);
        return checker.result;
    }

}

#endif