#pragma once
#ifndef SOLVER_POST_HELPER
#define SOLVER_POST_HELPER

#include <set>
#include "util/timer.hpp"
#include "encoder.hpp"
#include "flowgraph.hpp"
#include "candidates.hpp"

namespace engine {

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
        bool mustBePure = checks.preSpec.empty() || checkPureOnly;
        auto qv = encoding.encoder.QuantifiedVariable(encoding.graph.config.GetFlowValueType().sort);
        auto contains = [&encoding](auto& node, auto var, auto mode) {
            return encoding.EncodeNodeContains(node, var, mode);
        };

        // check purity
        z3::expr_vector preContains(encoding.encoder.context);
        z3::expr_vector postContains(encoding.encoder.context);
        for (const auto& node : encoding.graph.nodes) {
            preContains.push_back(encoding.encoder.MakeFlowContains(node.preKeyset, qv) && contains(node, qv, EMode::PRE));
            postContains.push_back(encoding.encoder.MakeFlowContains(node.postKeyset, qv) && contains(node, qv, EMode::POST));
        }
        auto isPure = z3::forall(qv, z3::mk_or(preContains) == z3::mk_or(postContains)); // TODO: also check non-containedness?
        checks.Add(isPure, [mustBePure,&checks](bool isPure){
            std::cout << " ** is pure=" << isPure << std::endl;
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
                othersPreContainedVec.push_back(encoding.encoder.MakeFlowContains(preKeys, qv) && contains(node, qv, EMode::PRE));
                othersPostContainedVec.push_back(encoding.encoder.MakeFlowContains(postKeys, qv) && contains(node, qv, EMode::POST));
                keyPreContainedVec.push_back(encoding.encoder.MakeFlowContains(preKeys, key) && contains(node, key, EMode::PRE));
                keyPostContainedVec.push_back(encoding.encoder.MakeFlowContains(postKeys, key) && contains(node, key, EMode::POST));
                keyPreNotContainedVec.push_back(encoding.encoder.MakeFlowContains(preKeys, key) && !contains(node, key, EMode::PRE));
                keyPostNotContainedVec.push_back(encoding.encoder.MakeFlowContains(postKeys, key) && !contains(node, key, EMode::POST));
            }
            auto othersUnchanged = z3::forall(qv, z3::implies(qv != key, z3::mk_or(othersPreContainedVec) == z3::mk_or(othersPostContainedVec)));
            auto isPreContained = z3::mk_or(keyPreContainedVec);
            auto isPostContained = z3::mk_or(keyPostContainedVec);
            auto isPreNotContained = z3::mk_or(keyPreNotContainedVec);
            auto isPostNotContained = z3::mk_or(keyPostNotContainedVec);

            // check pure
            checks.Add(isPure && isPreContained, [&checks, obligation](bool holds) {
                std::cout << " ** isPure && isPreContained=" << holds << std::endl;
                if (!holds || obligation->kind == heal::SpecificationAxiom::Kind::DELETE) return;
                bool fulfilled = obligation->kind == heal::SpecificationAxiom::Kind::CONTAINS; // key contained => contains: success, insertion: fail
                checks.postSpec.push_back(std::make_unique<heal::FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), fulfilled));
            });
            checks.Add(isPure && isPreNotContained, [&checks, obligation](bool holds) {
                std::cout << " ** isPure && isPreNotContained=" << holds << std::endl;
                if (!holds || obligation->kind == heal::SpecificationAxiom::Kind::INSERT) return;
                bool fulfilled = false; // key not contained => contains: fail, deletion: fail
                checks.postSpec.push_back(std::make_unique<heal::FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), fulfilled));
            });

//            // debug
//            for (const auto& node : encoding.graph.nodes) {
//                auto preFlow = encoding.encoder(node.preAllInflow);
//                auto postFlow = encoding.encoder(node.postAllInflow);
//                auto preKeys = encoding.encoder(node.preKeyset);
//                auto postKeys = encoding.encoder(node.postKeyset);
//
//                checks.Add(preFlow(key), [&node](bool holds) {
//                    std::cout << " ** " << node.address << " preFlow(key)=" << holds << std::endl;
//                });
//                checks.Add(postFlow(key), [&node](bool holds) {
//                    std::cout << " ** " << node.address << " postFlow(key)=" << holds << std::endl;
//                });
//                checks.Add(preKeys(key), [&node](bool holds) {
//                    std::cout << " ** " << node.address << " preKeys(key)=" << holds << std::endl;
//                });
//                checks.Add(postKeys(key), [&node](bool holds) {
//                    std::cout << " ** " << node.address << " postKeys(key)=" << holds << std::endl;
//                });
//                checks.Add(contains(node, key, EMode::PRE), [&node](bool holds) {
//                    std::cout << " ** " << node.address << " contains(key)=" << holds << std::endl;
//                });
//                checks.Add(!contains(node, key, EMode::PRE), [&node](bool holds) {
//                    std::cout << " ** " << node.address << " !contains(key)=" << holds << std::endl;
//                });
//
//                for (const auto& field : node.pointerFields) {
//                    auto preOut = encoding.encoder(*field.preAllOutflow);
//                    auto postOut = encoding.encoder(*field.postAllOutflow);
//                    checks.Add(preOut(key), [&node,&field](bool holds) {
//                        std::cout << " ** " << node.address << "::" << field.name << " preOut(key)=" << holds << std::endl;
//                    });
//                    checks.Add(postOut(key), [&node,&field](bool holds) {
//                        std::cout << " ** " << node.address << "::" << field.name << " postOut(key)=" << holds << std::endl;
//                    });
//                }
//            }
//            checks.Add(isPreContained, [](bool holds) {
//                std::cout << " ** isPreContained=" << holds << std::endl;
//            });
//            checks.Add(isPostContained, [](bool holds) {
//                std::cout << " ** isPostContained=" << holds << std::endl;
//            });
//            checks.Add(isPreNotContained, [](bool holds) {
//                std::cout << " ** isPreNotContained=" << holds << std::endl;
//            });
//            checks.Add(isPostNotContained, [](bool holds) {
//                std::cout << " ** isPostNotContained=" << holds << std::endl;
//            });

            // check impure
            if (checkPureOnly) continue;
            if (obligation->kind == heal::SpecificationAxiom::Kind::CONTAINS) continue; // always pure
            auto isInsertion = isPreNotContained && isPostContained && othersUnchanged;
            auto isDeletion = isPreContained && isPostNotContained && othersUnchanged;
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

    static void AddDerivedKnowledgeChecks(EncodedFlowGraph& encoding, FootprintChecks& checks, EMode mode) {
        checks.candidates = CandidateGenerator::Generate(encoding.graph, mode, CandidateGenerator::FlowLevel::FAST);
        for (std::size_t index = 0; index < checks.candidates.size(); ++index) {
            checks.Add(encoding.encoder(*checks.candidates[index]), [&checks,index,target=encoding.graph.pre->now.get()](bool holds){
//                std::cout << "  |implies=" << holds << "|  " << *checks.candidates[index] << std::endl;
                if (!holds) return;
                target->conjuncts.push_back(std::move(checks.candidates[index]));
            });
        }
    }

    static std::unique_ptr<heal::Annotation> ExtendStack(std::unique_ptr<heal::Annotation> annotation, const SolverConfig& config) {
        // TODO: use this method less! rather invoke it before interference
        std::cout << "%% extending stack" << std::endl;
        static Timer timer("engine::ExtendStack");
        auto measurement = timer.Measure();

        auto graph = engine::MakePureHeapGraph(std::move(annotation), config);
        if (graph.nodes.empty()) return std::move(graph.pre);
        EncodedFlowGraph encoding(std::move(graph));
        FootprintChecks checks(encoding.context);
        AddDerivedKnowledgeChecks(encoding, checks, EMode::PRE); // TODO: when to do this? what to derive (only flow? only info about symbols involved in points-to?)
        engine::ComputeImpliedCallback(encoding.solver, checks.checks);
        annotation = std::move(encoding.graph.pre);
        heal::InlineAndSimplify(*annotation);

//        auto candidates = CandidateGenerator::Generate(*annotation, CandidateGenerator::FlowLevel::NONE);
//        z3::context context;
//        z3::engine engine(context);
//        Z3Encoder encoder(context, engine);
//        engine.add(encoder(*annotation->now));
//        for (auto* memory : heal::CollectMemory(*annotation)) engine.add(encoder(*config.GetNodeInvariant(*memory)));
//        for (auto* resource : heal::CollectVariables(*annotation)) if (resource->variable->decl.is_shared) engine.add(encoder(*config.GetSharedVariableInvariant(*resource)));
//        z3::expr_vector vector(context);
//        for (const auto& candidate : candidates) vector.push_back(encoder(*candidate));
//        auto implied = ComputeImplied(engine, vector);
//        for (std::size_t index = 0; index < candidates.size(); ++index) {
//            if (!implied.at(index)) continue;
//            annotation->now->conjuncts.push_back(std::move(candidates.at(index)));
//        }
//        heal::Simplify(*annotation);

        return annotation;
    }

    static std::unique_ptr<heal::Annotation> TryAddPureFulfillment(std::unique_ptr<heal::Annotation> annotation, const SolverConfig& config, bool* isAnnotationUnsatisfiable= nullptr) {
        static Timer timer("engine::TryAddPureFulfillment");
        auto measurement = timer.Measure();

        auto graph = engine::MakePureHeapGraph(std::move(annotation), config);
        if (graph.nodes.empty()) return std::move(graph.pre);
        EncodedFlowGraph encoding(std::move(graph));
        FootprintChecks checks(encoding.context);
        checks.preSpec = heal::CollectObligations(*encoding.graph.pre->now);
        AddSpecificationChecks<true>(encoding, checks);
//        AddDerivedKnowledgeChecks(encoding, checks, EMode::PRE); // TODO: when to do this? what to derive (only flow? only info about symbols involved in points-to?)
        engine::ComputeImpliedCallback(encoding.solver, checks.checks, isAnnotationUnsatisfiable);
        annotation = std::move(encoding.graph.pre);
        annotation = RemoveObligations(std::move(annotation), std::move(checks.preSpec));
        std::move(checks.postSpec.begin(), checks.postSpec.end(), std::back_inserter(annotation->now->conjuncts));
        heal::InlineAndSimplify(*annotation);

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

}

#endif