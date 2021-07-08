#include "default_solver.hpp"

#include "z3++.h"
#include "eval.hpp"
#include "encoder.hpp"
#include "flowgraph.hpp"
#include "cola/util.hpp" // TODO: delete
#include "expand.hpp"
#include "post_helper.hpp"
#include "timer.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

/* NOTE:
 * This checks for acyclicity of the flow graph. One would prefer to check for non-empty flow cycles instead.
 * However, this would require to construct many flow graphs when extending the exploration frontier (detecting new points-to predicates).
 * Hence, we go with a syntactic check for reachability.
 */

/*
 * Histories: wenn predecessor mal flow gehabt + $interpolationsargument, dann hat successor/'value' auch mal flow gehabt
 */

//
// Checks
//

void CheckPublishing(FlowGraph& footprint) {
    for (auto& node : footprint.nodes) {
        if (node.preLocal == node.postLocal) continue;
        node.needed = true;
        for (auto& field : node.pointerFields) {
            auto next = footprint.GetNodeOrNull(field.postValue);
            if (!next) throw std::logic_error("Footprint too small to capture publishing"); // TODO: better error handling
            next->needed = true;
        }
    }
}

void CheckReachability(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    Reachability preReach(encoding.graph, EMode::PRE);
    Reachability postReach(encoding.graph, EMode::POST);

    // ensure no cycle inside footprint after update (potential such cycles must involve root)
    for (const auto& node : encoding.graph.nodes) {
        if (!postReach.IsReachable(node.address, node.address)) continue;
        throw std::logic_error("Cycle withing footprint detected. Cautiously aborting..."); // TODO: better error handling
    }

    // local nodes do not add cycles when pointed to shared
    auto& root = encoding.graph.GetRoot();
    if (root.preLocal) {
        bool newSuccessorsAreShared = true;
        for (const auto& field : root.pointerFields) {
            if (&field.postValue.get() == &field.preValue.get()) continue;
            auto successor = encoding.graph.GetNodeOrNull(field.postValue);
            newSuccessorsAreShared &= successor && !successor->preLocal;
        }
        if (newSuccessorsAreShared) return;
    }

    // ensure that no nodes outside the footprint are reachable after the update that were unreachable before
    auto preRootReachable = preReach.GetReachable(root.address);
    auto postRootReachable = postReach.GetReachable(root.address);
    for (const auto* address : postRootReachable) {
        if (preRootReachable.count(address) != 0) continue;
        if (encoding.graph.GetNodeOrNull(*address)) continue;
        auto isNull = encoding.encoder.MakeNullCheck(*address);
        checks.Add(isNull, [address](bool holds){
            if (holds) return;
            throw std::logic_error("Footprint too small to guarantee acyclicity of heap graph: potentially non-null address" + address->name + " may have become reachable."); // TODO: better error handling
        });
    }
}

void AddFlowCoverageChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    auto ensureFootprintContains = [&encoding](const SymbolicVariableDeclaration& address){
        // TODO: if address == NULL, then ignore
        auto mustHaveNode = encoding.graph.GetNodeOrNull(address);
        if (!mustHaveNode) throw std::logic_error("Footprint too small: does not cover addresses " + address.name + " the inflow of which changed."); // TODO: better error handling
        mustHaveNode->needed = true;
    };

    auto qv = encoding.encoder.QuantifiedVariable(encoding.graph.config.GetFlowValueType().sort);
    for (const auto& node : encoding.graph.nodes) {
        for (const auto& field : node.pointerFields) {
            auto sameFlowInner = encoding.encoder(field.preAllOutflow.value())(qv) == encoding.encoder(field.postAllOutflow.value())(qv); // TODO: all or graph flow?
//            auto sameFlowInner = encoding.encoder(field.preGraphOutflow.value())(qv) == encoding.encoder(field.postGraphOutflow.value())(qv);
            auto sameFlow = z3::forall(qv, sameFlowInner);
            auto isPreNull = encoding.encoder.MakeNullCheck(field.preValue);
            auto isPostNull = encoding.encoder.MakeNullCheck(field.postValue);
            checks.Add(sameFlow || isPreNull, [ensureFootprintContains,&node,&field](bool unchanged){
                std::cout << "outflow of " << node.address.name << " at " << field.name << " is preUnchangedOrNull=" << unchanged << std::endl;
                if (!unchanged) ensureFootprintContains(field.preValue);
            });
            checks.Add(sameFlow || isPostNull, [ensureFootprintContains,&node,&field](bool unchanged){
                std::cout << "outflow of " << node.address.name << " at " << field.name << " is postUnchangedOrNull=" << unchanged << std::endl;
                if (!unchanged) ensureFootprintContains(field.postValue);
            });
//            checks.Add(sameFlow, [ensureFootprintContains,&node,&field](bool unchanged){
//                std::cout << "outflow of " << node.address.name << " at " << field.name << " is unchanged=" << unchanged << std::endl;
//                if (unchanged) return;
//                ensureFootprintContains(field.preValue);
//                ensureFootprintContains(field.postValue);
//            });
        }
    }
}

//void AddFlowCycleChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
//    // Note: 'AddFlowCoverageChecks' guarantees that the update does not establish a non-empty flow cycle outside the footprint
//    // ensure absence of non-empty flow cycle inside footprint after update (such cycles must involve root)
//    for (const auto* field : encoding.graph.GetIncomingEdges(encoding.graph.GetRoot(), EMode::POST)) {
//        auto& flowField = field->postRootOutflow; // TODO: root or all flow?
//        assert(flowField.has_value());
//        InflowEmptinessAxiom tmp(flowField.value(), true);
//        checks.Add(encoding.encoder(tmp), [](bool noFlow){
//            if (noFlow) return;
//            throw std::logic_error("Potential flow cycle in footprint detected."); // TODO: better error handling
//        });
//    }
//}

void AddFlowUniquenessChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    auto disjointness = encoding.EncodeKeysetDisjointness(EMode::POST);
    checks.Add(disjointness, [](bool holds){
//        std::cout << "Checking keyset disjointness: holds=" << holds << std::endl;
        if (holds) return;
        throw std::logic_error("Unsafe update: keyset disjointness not guaranteed."); // TODO: better error handling
    });

    for (const auto& node : encoding.graph.nodes) {
        auto uniqueness = encoding.EncodeInflowUniqueness(node, EMode::POST);
        checks.Add(uniqueness, [&node](bool holds){
//            std::cout << "Checking inflow uniqueness for " << node.address.name << ": holds=" << holds << std::endl;
            if (holds) return;
            throw std::logic_error("Unsafe update: inflow uniqueness not guaranteed."); // TODO: better error handling
        });
    }
}

void AddInvariantChecks(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    for (const auto& node : encoding.graph.nodes) {
        auto nodeInvariant = encoding.EncodeNodeInvariant(node, EMode::POST);
        checks.Add(nodeInvariant, [&node](bool holds){
//            std::cout << "Checking invariant for " << node.address.name << ": holds=" << holds << std::endl;
            if (holds) return;
            throw std::logic_error("Unsafe update: invariant is not maintained."); // TODO: better error handling
        });
    }
}

//
// Perform Checks
//

void PerformChecks(EncodedFlowGraph& encoding, const FootprintChecks& checks) {
    solver::ComputeImpliedCallback(encoding.solver, checks.checks);
}

void MinimizeFootprint(FlowGraph& footprint) {
//    footprint.nodes.erase(std::remove_if(footprint.nodes.begin(), footprint.nodes.end(),
//                                         [](const auto& node) { return !node.needed; }), footprint.nodes.end());
    std::deque<FlowGraphNode> nodes;
    for (auto& node : footprint.nodes) {
        if (!node.needed) continue;
        nodes.push_back(std::move(node));
    }
    footprint.nodes = std::move(nodes);
}

//
// Post Image
//

struct PostUpdater : DefaultLogicNonConstListener {
    FlowGraph& footprint;
    explicit PostUpdater(FlowGraph& footprint) : footprint(footprint) {}

    void enter(PointsToAxiom& resource) override {
        auto node = footprint.GetNodeOrNull(resource.node->Decl());
        if (!node) return;
        auto update = node->ToLogic(EMode::POST);
        resource.flow = update->flow;
        resource.isLocal = update->isLocal;
        resource.fieldToValue = std::move(update->fieldToValue);
    }
};

std::unique_ptr<Annotation> ExtractPost(EncodedFlowGraph&& encoding, FootprintChecks&& checks) {
    PostUpdater updater(encoding.graph);
    auto result = std::move(encoding.graph.pre);
    result->now->accept(updater); // do not touch time predicates
    result = RemoveObligations(std::move(result), std::move(checks.preSpec)); // do not touch time predicates
    std::move(checks.postSpec.begin(), checks.postSpec.end(), std::back_inserter(result->now->conjuncts));
    return result;
}

//
// Extract Effects
//

std::vector<std::function<std::unique_ptr<Axiom>()>> GetContextGenerators(const VariableDeclaration& decl) {
    if (auto symbol = dynamic_cast<const SymbolicVariableDeclaration*>(&decl)) {
        switch (decl.type.sort) {
            case Sort::VOID: return {};
            case Sort::BOOL: return { // true or false
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(true)); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(false)); }
            };
            case Sort::DATA: return { // min, max, or between
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicMin>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicMax>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()); }
            };
            case Sort::PTR: return { // NULL or not
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::EQ, std::make_unique<SymbolicNull>()); },
                [symbol]() { return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(*symbol), SymbolicAxiom::NEQ, std::make_unique<SymbolicNull>()); }
            };
        }
    } else if (auto flow = dynamic_cast<const SymbolicFlowDeclaration*>(&decl)) {
        return { // empty or not
            [flow]() { return std::make_unique<InflowEmptinessAxiom>(*flow, true); },
            [flow]() { return std::make_unique<InflowEmptinessAxiom>(*flow, false); }
        };
    }
    return {};
}

void AddEffectContextGenerators(EncodedFlowGraph& encoding, FootprintChecks& checks) {
    // collect variables appearing in footprint
    std::set<const VariableDeclaration*> vars;
    for (const auto& node : encoding.graph.nodes) {
        auto dummy = node.ToLogic(EMode::PRE);
        vars.insert(&dummy->flow.get());
        for (const auto& [field, symbol] : dummy->fieldToValue) {
            vars.insert(&symbol->Decl());
        }
    }

    for (const auto* decl : vars) {
        for (auto&& generator : GetContextGenerators(*decl)) {
            auto candidate = encoding.encoder(*generator());
            checks.Add(candidate, [decl,generator,&checks](bool holds){
//                std::cout << "Testing "; heal::Print(*generator(), std::cout); std::cout << " ==> holds=" << holds << std::endl;
                if (holds) checks.context[decl].conjuncts.push_back(generator());
            });
        }
    }
}

std::unique_ptr<Formula> GetNodeUpdateContext(const PointsToAxiom& node, FootprintChecks::ContextMap& contextMap) {
    auto result = std::make_unique<SeparatingConjunction>();
    auto handle = [&result,&contextMap](const auto& decl) {
        auto knowledge = std::make_unique<SeparatingConjunction>();
        knowledge->conjuncts.swap(contextMap[&decl].conjuncts);
        result->conjuncts.push_back(std::move(knowledge));
    };

    handle(node.flow.get());
    for (const auto& [name, symbol] : node.fieldToValue) {
        handle(symbol->Decl());
    }

    heal::Simplify(*result);
    return result;
}

std::unique_ptr<HeapEffect> ExtractEffect(const FlowGraphNode& node, FootprintChecks::ContextMap& contextMap) {
    // ignore local nodes; after minimization, all remaining footprint nodes contain changes
    if (node.preLocal) return nullptr;
    auto pre = node.ToLogic(EMode::PRE);
    auto post = node.ToLogic(EMode::POST);
    auto context = GetNodeUpdateContext(*pre, contextMap);
    return std::make_unique<HeapEffect>(std::move(pre), std::move(post), std::move(context));
}

std::deque<std::unique_ptr<HeapEffect>> ExtractEffects(const EncodedFlowGraph& encoding, FootprintChecks::ContextMap&& contextMap) {
    // create a separate effect for every node
    std::deque<std::unique_ptr<HeapEffect>> result;
    for (const auto& node : encoding.graph.nodes) {
        auto effect = ExtractEffect(node, contextMap);
        if (!effect) continue;
//        std::cout << "Extracted effect: " << *effect->pre << " ~~> " << *effect->post << "  under  " << *effect->context << std::endl;
        assert(effect);
        assert(effect->pre);
        assert(effect->post);
        assert(effect->context);
        result.push_back(std::move(effect));
    }
    return result;
}

//
// Overall Algorithm
//

PostImage DefaultSolver::PostMemoryUpdate(std::unique_ptr<Annotation> pre, const MultiUpdate& update) const {
    static Timer timer("DefaultSolver::PostMemoryUpdate");
    auto measurement = timer.Measure();

    // TODO: use future
    // TODO: create and use update/effect lookup table
    // TODO: start with smaller footprint and increase if too small?

//    // begin debug
//    std::cout << std::endl << std::endl << std::endl << "====================" << std::endl;
//    std::cout << "== Command: "; cola::print(cmd, std::cout);
//    std::cout << "== Pre: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl << std::flush;
//    // end debug

    heal::InlineAndSimplify(*pre);
    for (const auto& [lhs, rhs] : update) pre->now = solver::ExpandMemoryFrontierForAccess(std::move(pre->now), Config(), *lhs);
    FlowGraph footprint = solver::MakeFlowFootprint(std::move(pre), update, Config());
    std::cout << "** pre after footprint creation: " << *footprint.pre << std::endl;
    EncodedFlowGraph encoding(std::move(footprint));
    FootprintChecks checks(encoding.context);
    checks.preSpec = heal::CollectObligations(*encoding.graph.pre->now);

//    // debug
//    for (const auto& node : encoding.graph.nodes) {
//        auto inf = encoding.encoder(*std::make_unique<SymbolicMax>());
//        auto qv = encoding.encoder.QuantifiedVariable(Config().GetFlowValueType().sort);
//        checks.Add(encoding.encoder(node.preAllInflow)(inf), [&node](bool holds) {
//            std::cout << " ** " << node.address << " preInflow(inf)=" << holds << std::endl;
//        });
//        checks.Add(z3::forall(qv, !encoding.encoder(node.frameInflow)(inf)), [&node](bool holds) {
//            std::cout << " ** " << node.address << " frameInflow notexists=" << holds << std::endl;
//        });
//        checks.Add(z3::exists(qv, encoding.encoder(node.frameInflow)(inf)), [&node](bool holds) {
//            std::cout << " ** " << node.address << " frameInflow exists=" << holds << std::endl;
//        });
//        checks.Add(z3::exists(qv, encoding.encoder(node.preGraphInflow)(inf)), [&node](bool holds) {
//            std::cout << " ** " << node.address << " preGraphInflow exists=" << holds << std::endl;
//        });
//        checks.Add(z3::exists(qv, encoding.encoder(node.postGraphInflow)(inf)), [&node](bool holds) {
//            std::cout << " ** " << node.address << " postGraphInflow exists=" << holds << std::endl;
//        });
//        for (const auto& field : node.pointerFields) {
//            auto key = encoding.encoder.QuantifiedVariable(field.type.sort);
//            auto preOut = encoding.encoder(*field.preAllOutflow);
//            auto postOut = encoding.encoder(*field.postAllOutflow);
//            checks.Add(preOut(inf), [&node,&field](bool holds) {
//                std::cout << " ** " << node.address << "::" << field.name << " preOut(inf)=" << holds << std::endl;
//            });
//            checks.Add(postOut(inf), [&node,&field](bool holds) {
//                std::cout << " ** " << node.address << "::" << field.name << " postOut(inf)=" << holds << std::endl;
//            });
//            checks.Add(z3::exists(key, preOut(key)), [&node,&field](bool holds) {
//                std::cout << " ** " << node.address << "::" << field.name << " preOut(key)=" << holds << std::endl;
//            });
//            checks.Add(z3::exists(key, postOut(key)), [&node,&field](bool holds) {
//                std::cout << " ** " << node.address << "::" << field.name << " postOut(key)=" << holds << std::endl;
//            });
//        }
//    }

    CheckPublishing(encoding.graph);
    CheckReachability(encoding, checks);
//    AddDerivedKnowledgeChecks(encoding, checks, EMode::PRE);
    AddFlowCoverageChecks(encoding, checks);
    // AddFlowCycleChecks(encoding, checks);
    AddFlowUniquenessChecks(encoding, checks);
    AddSpecificationChecks(encoding, checks);
    AddInvariantChecks(encoding, checks);
//    AddDerivedKnowledgeChecks(encoding, checks, EMode::POST); // TODO: is this correct?
    AddEffectContextGenerators(encoding, checks);
    PerformChecks(encoding, checks);

    MinimizeFootprint(encoding.graph);
    auto effects = ExtractEffects(encoding, std::move(checks.context));
    auto post = ExtractPost(std::move(encoding), std::move(checks));

    // begin debug
    heal::Print(*post, std::cout); std::cout << std::endl;
//    std::cout << "== Post: " << std::endl; heal::Print(*post, std::cout); std::cout << std::endl;
//    std::cout << "== Effects: " << std::endl;
//    for (auto& effect : effects) {
//        std::cout << "   - ";
//        heal::Print(*effect->pre, std::cout);
//        std::cout << "  ~~~>  ";
//        heal::Print(*effect->post, std::cout);
//        std::cout << "  UNDER  ";
//        heal::Print(*effect->context, std::cout);
//        std::cout << std::endl;
//    }
    // end debug

    assert(heal::CollectObligations(*post).size() + heal::CollectFulfillments(*post).size() > 0);
    return PostImage(std::move(post), std::move(effects));
}