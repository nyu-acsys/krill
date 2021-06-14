#pragma once
#ifndef SOLVER_POST_HELPER
#define SOLVER_POST_HELPER

#include <set>
#include "encoder.hpp"
#include "flowgraph.hpp"
#include "candidates.hpp"

namespace solver {

    struct FootprintChecks {
        using ContextMap = std::map<const cola::VariableDeclaration*, heal::SeparatingConjunction>;

        ImplicationCheckSet checks;
        std::set<const heal::ObligationAxiom*> preSpec;
        std::deque<std::unique_ptr<heal::SpecificationAxiom>> postSpec;
        ContextMap context;
        std::deque<std::unique_ptr<heal::Axiom>> candidates;

        FootprintChecks(const FootprintChecks& other) = delete;
        explicit FootprintChecks(z3::context& context_) : checks(context_) {}
        void Add(const z3::expr& expr, std::function<void(bool)> callback) { checks.Add(expr, std::move(callback)); }
    };

    static inline std::unique_ptr<heal::Annotation> RemoveObligations(std::unique_ptr<heal::Annotation> annotation, std::set<const heal::ObligationAxiom*>&& obligations) {
        static struct : public heal::BaseNonConstResourceVisitor {
            std::set<const heal::ObligationAxiom*> destroy;
            void enter(heal::SeparatingConjunction& obj) override {
                obj.conjuncts.erase(std::remove_if(obj.conjuncts.begin(), obj.conjuncts.end(), [this](const auto& elem){
                    auto obligation = dynamic_cast<const heal::ObligationAxiom*>(elem.get()); // TODO: avoid cast
                    return obligation && destroy.count(obligation) != 0;
                }), obj.conjuncts.end());
            }
        } remover;
        remover.destroy = std::move(obligations);
        annotation->accept(remover);
        return annotation;
    }

    template<bool checkPureOnly=false>
    static void AddSpecificationChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
        bool mustBePure = checks.preSpec.empty();
        auto qv = encoding.encoder.QuantifiedVariable(encoding.graph.config.GetFlowValueType().sort);
        auto contains = [&encoding](auto& node, auto var, auto mode) {
            return encoding.EncodeNodeContains(node, var, mode);
        };

        // check purity
        z3::expr_vector preContains(encoding.encoder.context);
        z3::expr_vector postContains(encoding.encoder.context);
        for (const auto& node : encoding.graph.nodes) {
            preContains.push_back(encoding.encoder(node.preKeyset)(qv) && contains(node, qv, EMode::PRE));
            postContains.push_back(encoding.encoder(node.postKeyset)(qv) && contains(node, qv, EMode::POST));
        }
        auto isPure = z3::forall(qv, z3::mk_or(preContains) == z3::mk_or(postContains));
        checks.Add(isPure, [mustBePure,&checks](bool isPure){
            if (!isPure && mustBePure) throw std::logic_error("Unsafe update: impure update without obligation."); // TODO: better error handling
            if (!isPure) return;
            for (const auto* obligation : checks.preSpec) {
                checks.postSpec.push_back(heal::Copy(*obligation));
            }
        });

        // impure updates require obligation, checked together with purity
        for (const auto* obligation : checks.preSpec){
            assert(obligation);
            auto key = encoding.encoder(*obligation->key);

            // prepare
            z3::expr_vector othersPreContainedVec(encoding.encoder.context);
            z3::expr_vector othersPostContainedVec(encoding.encoder.context);
            z3::expr_vector keyPreContainedVec(encoding.encoder.context);
            z3::expr_vector keyPostContainedVec(encoding.encoder.context);
            z3::expr_vector keyPreNotContainedVec(encoding.encoder.context);
            z3::expr_vector keyPostNotContainedVec(encoding.encoder.context);
            for (const auto &node : encoding.graph.nodes) {
                auto preKeys = encoding.encoder(node.preKeyset);
                auto postKeys = encoding.encoder(node.postKeyset);
                othersPreContainedVec.push_back(preKeys(qv) && contains(node, qv, EMode::PRE));
                othersPostContainedVec.push_back(postKeys(qv) && contains(node, qv, EMode::POST));
                keyPreContainedVec.push_back(preKeys(key) && contains(node, key, EMode::PRE));
                keyPostContainedVec.push_back(postKeys(key) && contains(node, key, EMode::POST));
                keyPreNotContainedVec.push_back(preKeys(key) && !contains(node, key, EMode::PRE));
                keyPostNotContainedVec.push_back(postKeys(key) && !contains(node, key, EMode::POST));
            }
            auto othersUnchanged = z3::forall(qv, z3::implies(qv != key, z3::mk_or(othersPreContainedVec) == z3::mk_or(othersPostContainedVec)));
            auto isPreContainedNew = z3::mk_or(keyPreContainedVec);
            auto isPostContainedNew = z3::mk_or(keyPostContainedVec);
            auto isPreNotContainedNew = z3::mk_or(keyPreNotContainedVec);
            auto isPostNotContainedNew = z3::mk_or(keyPostNotContainedVec);


            // check pure
            checks.Add(isPure && isPreContainedNew, [&checks, obligation](bool holds) {
                std::cout << "     >-> isContained=" << holds << std::endl;
                if (!holds || obligation->kind == heal::SpecificationAxiom::Kind::DELETE) return;
                bool fulfilled = obligation->kind == heal::SpecificationAxiom::Kind::CONTAINS; // key contained => contains: success, insertion: fail
                checks.postSpec.push_back(std::make_unique<heal::FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), fulfilled));
            });
            checks.Add(isPure && isPreNotContainedNew, [&checks, obligation](bool holds) {
                std::cout << "     >-> isNotContained=" << holds << std::endl;
                if (!holds || obligation->kind == heal::SpecificationAxiom::Kind::INSERT) return;
                bool fulfilled = false; // key not contained => contains: fail, deletion: fail
                checks.postSpec.push_back(std::make_unique<heal::FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), fulfilled));
            });

            // check impure
            if (checkPureOnly) continue;
            if (obligation->kind == heal::SpecificationAxiom::Kind::CONTAINS) continue; // always pure
            auto isInsertion = isPreNotContainedNew && isPostContainedNew && othersUnchanged;
            auto isDeletion = isPreContainedNew && isPostNotContainedNew && othersUnchanged;
            auto isUpdate = obligation->kind == heal::SpecificationAxiom::Kind::INSERT ? isInsertion : isDeletion;
            checks.Add(isUpdate, [&checks, obligation](bool isTarget) {
                if (isTarget) {
                    checks.postSpec.push_back(std::make_unique<heal::FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), true));
                } else if (checks.postSpec.empty()) {
                    throw std::logic_error("Unsafe update: impure update that does not satisfy the specification");
                }
            });
        }
    }

    static void AddDerivedKnowledgeChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
        checks.candidates = CandidateGenerator::Generate(*encoding.graph.pre, CandidateGenerator::FlowLevel::FAST);
        for (std::size_t index = 0; index < checks.candidates.size(); ++index) {
            checks.Add(encoding.encoder(*checks.candidates[index]), [&checks,index,target=encoding.graph.pre->now.get()](bool holds){
                if (!holds) return;
                target->conjuncts.push_back(std::move(checks.candidates[index]));
            });
        }
    }

    static std::unique_ptr<heal::Annotation> TryAddPureFulfillment(std::unique_ptr<heal::Annotation> annotation, const heal::SymbolicVariableDeclaration& address, const SolverConfig& config) {
        std::cout << " >>> pure fulfillment check for " << address.name << std::endl;
//        std::cout << std::endl << std::endl << "******* pure fulfillment check" << std::endl;
//        heal::Print(*annotation, std::cout);
//        std::cout << "address: " << address.name << std::endl << std::endl;

        EncodedFlowGraph encoding(solver::MakeFlowGraph(std::move(annotation), address, config));
        FootprintChecks checks(encoding.context);
        checks.preSpec = heal::CollectObligations(*encoding.graph.pre->now);
        AddSpecificationChecks<true>(encoding, checks);
        AddDerivedKnowledgeChecks(encoding, checks);
        solver::ComputeImpliedCallback(encoding.solver, checks.checks);
        annotation = std::move(encoding.graph.pre);
        annotation = RemoveObligations(std::move(annotation), std::move(checks.preSpec));
        std::move(checks.postSpec.begin(), checks.postSpec.end(), std::back_inserter(annotation->now->conjuncts));
        return annotation;
    }

    static inline std::set<const cola::Dereference*> GetDereferences(const cola::Expression& expression) { // TODO: remove / move to better location
        struct Collector : public cola::BaseVisitor {
            std::set<const cola::Dereference*> result;
            void visit(const cola::BooleanValue& /*node*/) override { /* do nothing */ }
            void visit(const cola::NullValue& /*node*/) override { /* do nothing */ }
            void visit(const cola::EmptyValue& /*node*/) override { /* do nothing */ }
            void visit(const cola::MaxValue& /*node*/) override { /* do nothing */ }
            void visit(const cola::MinValue& /*node*/) override { /* do nothing */ }
            void visit(const cola::NDetValue& /*node*/) override { /* do nothing */ }
            void visit(const cola::VariableExpression& /*node*/) override { /* do nothing */ }
            void visit(const cola::NegatedExpression& node) override { node.expr->accept(*this); }
            void visit(const cola::BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
            void visit(const cola::Dereference& node) override { node.expr->accept(*this); result.insert(&node); }
        };
        Collector collector;
        expression.accept(collector);
        return std::move(collector.result);
    }
//
//    static std::unique_ptr<heal::Annotation> TryAddPureFulfillment(std::unique_ptr<heal::Annotation> annotation, const cola::Expression& expression, const SolverConfig& config) {
//        LazyValuationMap eval(*annotation);
//        for (const auto* dereference : GetDereferences(expression)) {
//            auto resource = solver::FindResource(*dereference, *annotation); // TODO: lazy
//            if (!resource) continue;
//            annotation = TryAddPureFulfillment(std::move(annotation), resource->node->Decl(), config); // TODO: avoid repeated solver invocations
//        }
//        return annotation;
//    }

    static std::unique_ptr<heal::Annotation> TryAddPureFulfillment(std::unique_ptr<heal::Annotation> annotation, const SolverConfig& config) {
        auto resources = heal::CollectMemory(*annotation);
        for (const auto* memory : resources) {
            annotation = TryAddPureFulfillment(std::move(annotation), memory->node->Decl(), config); // TODO: avoid repeated solver invocations
        }
        return annotation;
    }

}

#endif