#include "default_solver.hpp"

#include "z3++.h"
#include "eval.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

constexpr const std::size_t FOOTPRINT_DEPTH = 1; // TODO: value should come from flow/config

enum struct Mode { PRE, POST };


struct Field { // TODO: delete copy constructor to avoid accidental copying?
    const std::string name;
    const Type& type;
    std::reference_wrapper<const SymbolicVariableDeclaration> preValue;
    std::reference_wrapper<const SymbolicVariableDeclaration> postValue;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> preAllOutflow;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> preRootOutflow;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> postAllOutflow;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> postRootOutflow;
};

struct FootprintNode { // TODO: delete copy constructor to avoid accidental copying?
    const SymbolicVariableDeclaration& address;
    bool needed;
    bool preLocal;
    bool postLocal;
    std::reference_wrapper<const SymbolicFlowDeclaration> preAllInflow;
    std::reference_wrapper<const SymbolicFlowDeclaration> preRootInflow;
    std::reference_wrapper<const SymbolicFlowDeclaration> preKeyset;
    std::reference_wrapper<const SymbolicFlowDeclaration> postAllInflow;
    std::reference_wrapper<const SymbolicFlowDeclaration> postRootInflow;
    std::reference_wrapper<const SymbolicFlowDeclaration> postKeyset;
    std::reference_wrapper<const SymbolicFlowDeclaration> frameInflow;
    std::vector<Field> dataFields;
    std::vector<Field> pointerFields;
};

struct Footprint { // TODO: delete copy constructor to avoid accidental copying?
    std::deque<FootprintNode> nodes;
    SymbolicFactory factory;
    std::unique_ptr<Annotation> pre;
};

std::unique_ptr<Formula> InstantiateInvariant(const Footprint& footprint, const FootprintNode& node, Mode mode) {
    throw std::logic_error("not yet implemented");
}

std::unique_ptr<Formula> InstantiateOutflow(LazyValuationMap& eval, const FootprintNode& node, const Field& field, Mode mode) {
    throw std::logic_error("not yet implemented");
}

FootprintNode* GetNodeOrNull(Footprint& footprint, const SymbolicVariableDeclaration& address) {
    // search for an existing node at the given address
    auto pred = [&address](const auto& item){ item.address == address; };
    auto find = std::find_if(footprint.nodes.begin(), footprint.nodes.end(), pred);
    if (find != footprint.nodes.end()) return &(*find);
    return nullptr;
}

//
// Create Footprint, updating local bits and invoking invariant for nodes
//

FootprintNode* GetOrCreateNode(Footprint& footprint, LazyValuationMap& eval, const SymbolicVariableDeclaration& nextAddress) {
    // search for an existing node at the given address
    if (auto find = GetNodeOrNull(footprint, nextAddress)) return find;

    // abort if we do not have the resource for expanding the footprint
    auto* resource = eval.GetMemoryResourceOrNull(nextAddress);
    if (!resource) return nullptr;

    // create a new node
    auto& flowType = resource->flow.get().type;
    auto& preRootInflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& postRootInflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& postAllInflow = footprint.factory.GetUnusedFlowVariable(flowType); // flow might have changed
    auto& preKeyset = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& postKeyset = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& frameInflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    FootprintNode result{ nextAddress, false, resource->isLocal, resource->isLocal, resource->flow, preRootInflow, preKeyset,
                          postAllInflow, postRootInflow, postKeyset, frameInflow, {}, {} };
    result.dataFields.reserve(resource->fieldToValue.size());
    result.pointerFields.reserve(resource->fieldToValue.size());
    for (const auto& [name, value] : resource->fieldToValue) {
        Field field{ name, value->Type(), value->Decl(), value->Decl(), std::nullopt, std::nullopt, std::nullopt, std::nullopt };
        if (value->Sort() == Sort::PTR) {
            field.preAllOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            field.postRootOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            field.preAllOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            field.postRootOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            result.pointerFields.push_back(std::move(field));
        } else {
            result.dataFields.push_back(std::move(field));
        }
    }

    // add to footprint
    footprint.nodes.push_back(std::move(result));
    footprint.pre->now->conjuncts.push_back(InstantiateInvariant(footprint, footprint.nodes.back(), Mode::PRE));
    return &footprint.nodes.back();
}

void ExpandFootprint(Footprint& footprint, LazyValuationMap& eval, FootprintNode& node, std::size_t depth) {
    if (depth == 0) return;
    for (auto& field : node.pointerFields) {
        auto descend = [&](const auto& nextAddress) {
            auto* nextNode = GetOrCreateNode(footprint, eval, nextAddress);
            if (!nextNode) return;
            if (!node.postLocal) nextNode->postLocal = false; // publish node
            ExpandFootprint(footprint, eval, *nextNode, depth-1);
        };
        descend(field.preValue);
        descend(field.postValue);
    }
}

Field& GetFieldOrFail(FootprintNode& node, const std::string& name) {
    for (auto& field : node.pointerFields) {
        if (field.name == name) return field;
    }
    for (auto& field : node.dataFields) {
        if (field.name == name) return field;
    }
    throw std::logic_error("Could not find field."); // TODO: better error handling
}

Footprint MakeFootprint(std::unique_ptr<Annotation> pre_, const VariableDeclaration& lhsVar, const std::string& lhsField, const SimpleExpression& rhs) {
    Footprint footprint = { {}, SymbolicFactory(*pre_), std::move(pre_) };
    LazyValuationMap eval(*footprint.pre);

    // get variable for rhs value
    const SymbolicVariableDeclaration* rhsEval;
    if (auto var = dynamic_cast<const VariableExpression*>(&rhs)) {
        rhsEval = &eval.GetValueOrFail(var->decl);
    } else {
        auto rhsValue = eval.Evaluate(rhs);
        rhsEval = &footprint.factory.GetUnusedSymbolicVariable(rhs.type());
        auto axiom = std::make_unique<SymbolicAxiom>(std::move(rhsValue), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(*rhsEval));
        footprint.pre->now->conjuncts.push_back(std::move(axiom));
    }

    // make root node (update lhsField, do not change flow)
    FootprintNode* root = GetOrCreateNode(footprint, eval, eval.GetValueOrFail(lhsVar));
    if (!root) throw std::logic_error("Cannot initialize footprint."); // TODO: better error handling
    root->needed = true;
    root->preRootInflow = root->preAllInflow;
    root->postRootInflow = root->preAllInflow;
    root->postAllInflow = root->preAllInflow;
    GetFieldOrFail(*root, lhsField).postValue = *rhsEval;

    // generate flow graph
    ExpandFootprint(footprint, eval, *root, FOOTPRINT_DEPTH); // TODO: start with smaller footprint and increase if too small?
    return footprint;
}

//
// Encode footprint
//

struct ResourceCollector : public DefaultLogicListener {
    std::set<const SymbolicVariableDeclaration*> resources;
    void enter(const PointsToAxiom& obj) override { resources.insert(&obj.node->Decl()); }
};

struct FootprintEncoder : public BaseLogicVisitor {
    static constexpr int NULL_VALUE = 0;
    static constexpr int MIN_VALUE = -65536;
    static constexpr int MAX_VALUE = 65536;

    z3::context& context;
    z3::expr result;
    std::map<const SymbolicVariableDeclaration*, z3::expr> symbols;
    std::map<const SymbolicFlowDeclaration*, z3::func_decl> flows;

    explicit FootprintEncoder(z3::context& context) : context(context), result(context) {}

    z3::expr operator()(const LogicObject& obj) { return Encode(obj); }
    z3::expr operator()(const SymbolicVariableDeclaration& decl) { return EncodeSymbol(decl); }
    z3::func_decl operator()(const SymbolicFlowDeclaration& decl) { return EncodeFlow(decl); }

    void visit(const SymbolicVariable& obj) override { result = EncodeSymbol(obj.Decl()); }
    void visit(const SymbolicBool& obj) override { result = context.bool_val(obj.value); }
    void visit(const SymbolicNull& /*obj*/) override { result = context.int_val(NULL_VALUE); }
    void visit(const SymbolicMin& /*obj*/) override { result = context.int_val(MIN_VALUE); }
    void visit(const SymbolicMax& /*obj*/) override { result = context.int_val(MAX_VALUE); }
    void visit(const PointsToAxiom& /*obj*/) override { result = context.bool_val(true); }
    void visit(const EqualsToAxiom& /*obj*/) override { result = context.bool_val(true); }
    void visit(const ObligationAxiom& /*obj*/) override { result = context.bool_val(true); }
    void visit(const FulfillmentAxiom& /*obj*/) override { result = context.bool_val(true); }
    void visit(const Annotation& obj) override { result = Encode(*obj.now); }

    void visit(const InflowContainsValueAxiom& obj) override {
        result = EncodeFlow(obj.flow)(Encode(*obj.value)) == context.bool_val(true);
    }
    void visit(const InflowContainsRangeAxiom& obj) override {
        auto qv = context.int_const("__qv");
        auto premise = (Encode(*obj.valueLow) < qv) && (qv <= Encode(*obj.valueHigh));
        auto conclusion = EncodeFlow(obj.flow)(qv);
        result = z3::forall(qv, z3::implies(premise, conclusion));
    }
    void visit(const InflowEmptinessAxiom& obj) override {
        auto qv = context.int_const("__qv");
        result = z3::exists(qv, EncodeFlow(obj.flow)(qv));
        if (obj.isEmpty) result = !result;
    }

    void visit(const SeparatingConjunction& obj) override {
        result = z3::mk_and(EncodeAll(obj.conjuncts)) && MakeResourceGuarantee(obj);
    }
    void visit(const FlatSeparatingConjunction& obj) override {
        result = z3::mk_and(EncodeAll(obj.conjuncts)) && MakeResourceGuarantee(obj);
    }
    void visit(const StackDisjunction& obj) override {
        result = z3::mk_or(EncodeAll(obj.axioms));
    }
    void visit(const SeparatingImplication& obj) override {
        result = z3::implies(Encode(*obj.premise), Encode(*obj.conclusion));
    }
    void visit(const SymbolicAxiom& obj) override {
        auto lhs = Encode(*obj.lhs);
        auto rhs = Encode(*obj.rhs);
        switch (obj.op) {
            case SymbolicAxiom::EQ: result = lhs == rhs; break;
            case SymbolicAxiom::NEQ: result = lhs != rhs; break;
            case SymbolicAxiom::LEQ: result = lhs <= rhs; break;
            case SymbolicAxiom::LT: result = lhs < rhs; break;
            case SymbolicAxiom::GEQ: result = lhs >= rhs; break;
            case SymbolicAxiom::GT: result = lhs > rhs; break;
        }
    }

private:
    template<typename K, typename V, typename F>
    V GetOrCreate(std::map<K, V>& map, K key, F create) {
        auto find = map.find(key);
        if (find != map.end()) return find->second;
        auto newValue = create();
        map.insert({ key, newValue });
        return newValue;
    }

    z3::expr EncodeSymbol(const SymbolicVariableDeclaration& decl) {
        return GetOrCreate(symbols, &decl, [this,&decl](){
            auto name = "_v" + decl.name;
            return context.int_const(name.c_str());
        });
    }

    z3::func_decl EncodeFlow(const SymbolicFlowDeclaration& decl) {
        return GetOrCreate(flows, &decl, [this,&decl](){
            auto name = "_V" + decl.name;
            return context.function(name.c_str(), context.int_sort(), context.bool_sort());
        });
    }

    z3::expr Encode(const LogicObject& obj) {
        obj.accept(*this);
        return result;
    }

    template<typename T>
    z3::expr_vector EncodeAll(const T& elements) {
        z3::expr_vector vec(context);
        for (const auto& elem : elements) {
            vec.push_back(Encode(*elem));
        }
        return vec;
    }

    z3::expr MakeResourceGuarantee(const LogicObject& obj) {
        ResourceCollector collector;
        obj.accept(collector);
        z3::expr_vector vec(context);
        for (const auto* resource : collector.resources) {
            vec.push_back(EncodeSymbol(*resource));
        }
        return z3::distinct(vec);
    }
};

z3::expr EncodeOutflow(const Footprint& footprint, FootprintEncoder& encoder, const FootprintNode& node, const Field& field, Mode mode) {
//    successor inflow contains x if node sends x via field

// TODO: populate pre/post keyset
// TODO: populate pre/post rootinflow?
// TODO: populate pre/post rootoutflow?
// TODO: populate pre/post alloutflow?

//    auto qv = encoder.context.int_const("__qv");
//    auto inflow = encode(forPre ? node.preRootInflow : node.postRootInflow);
//    auto outflow = encoder(InstantiateOutflow(eval, node, field, forPre));
//    return z3::forall(qv, z3::implies(inflow, outflow));
    throw std::logic_error("not yet implemented");
}

z3::expr EncodeFlowRules(FootprintEncoder& encoder, const FootprintNode& node) {
    auto qv = encoder.context.int_const("__qv");
    z3::expr_vector result(encoder.context);
    auto addRule = [&qv, &result](auto pre, auto imp) { result.push_back(z3::forall(qv, z3::implies(pre, imp))); };

    z3::expr_vector preOuts(encoder.context), postOuts(encoder.context);
    for (const auto& field : node.pointerFields) {
        preOuts.push_back(encoder(*field.preAllOutflow)(qv));
        postOuts.push_back(encoder(*field.postAllOutflow)(qv));
    }

    auto preRoot = encoder(node.preRootInflow)(qv);
    auto postRoot = encoder(node.postRootInflow)(qv);
    auto preAll = encoder(node.preAllInflow)(qv);
    auto postAll = encoder(node.postAllInflow)(qv);
    auto preKey = encoder(node.preKeyset)(qv);
    auto postKey = encoder(node.postKeyset)(qv);
    auto frame = encoder(node.frameInflow)(qv);
    auto preOut = z3::mk_or(preOuts);
    auto postOut = z3::mk_or(postOuts);

    // root flow is subset of all flow
    addRule(preRoot, preAll);
    addRule(postRoot, postAll);

    // frame flow is subset of all flow
    addRule(frame, preAll);
    addRule(frame, postAll);

    // all flow is either frame flow or root flow
    addRule(preAll, preRoot || frame);
    addRule(postAll, postRoot || frame);

    // keyset is subset of all flow
    addRule(preKey, preAll);
    addRule(preKey, postAll);

    // keyset is inflow minus outflow
    addRule(preAll && !preOuts, preKey);
    addRule(postAll && !postOuts, postKey);

    return mk_and(result);
}

z3::expr EncodeKeysetDisjointness(const Footprint& footprint, FootprintEncoder& encoder, Mode mode) {
    auto qv = encoder.context.int_const("__qv");
    z3::expr_vector preKeyset(encoder.context);
    for (const auto& node : footprint.nodes) {
        auto& keyset = mode == Mode::PRE ? node.preKeyset.get() : node.postKeyset.get();
        preKeyset.push_back(encoder(keyset)(qv));
    }
    return z3::forall(qv, z3::distinct(preKeyset));
}

z3::expr EncodeInflowUniqueness(const Footprint& footprint, FootprintEncoder& encoder, Mode mode) {
    // create predecessor map
    std::map<const SymbolicVariableDeclaration*, std::set<const SymbolicFlowDeclaration*>> incoming;
    for (const auto& node : footprint.nodes) {
        for (const auto& field : node.pointerFields) {
            auto& outflow = mode == Mode::PRE ? field.preRootOutflow->get() : field.postRootOutflow->get();
            incoming[&node.address].insert(&outflow);
        }
    }

    // uniqueness
    auto qv = encoder.context.int_const("__qv");
    z3::expr_vector result(encoder.context);

    for (const auto& node : footprint.nodes) {
        z3::expr_vector inflow(encoder.context);
        inflow.push_back(encoder(node.frameInflow)(qv));
        for (auto& incomingFlow : incoming[&node.address]) {
            inflow.push_back(encoder(*incomingFlow)(qv));
        }
        result.push_back(z3::forall(qv, z3::distinct(inflow)));
    }

    return z3::mk_and(result);
}

z3::expr EncodeFlow(const Footprint& footprint, FootprintEncoder& encoder) {
    z3::expr_vector result(encoder.context);
    result.push_back(encoder(*footprint.pre));
    auto qv = encoder.context.int_const("__qv");

    // same inflow in root before and after update
    auto& root = footprint.nodes.front();
    assert(&root.preAllInflow.get() == &root.preRootInflow.get());
    assert(&root.preAllInflow.get() == &root.postRootInflow.get());
    assert(&root.preAllInflow.get() == &root.postAllInflow.get());

    // go over graph and encode flow
    for (const auto& node : footprint.nodes) {
        result.push_back(EncodeFlowRules(encoder, node));

        // outflow
        for (const auto& field : node.pointerFields) {
            result.push_back(EncodeOutflow(footprint, encoder, node, field, Mode::PRE));
            result.push_back(EncodeOutflow(footprint, encoder, node, field, Mode::POST));
        }
    }

    // add assumptions for pre flow: keysets are disjoint, inflow is unique
    result.push_back(EncodeKeysetDisjointness(footprint, encoder, Mode::PRE));
    result.push_back(EncodeInflowUniqueness(footprint, encoder, Mode::POST));

    return z3::mk_and(result);
}

struct FootprintEncoding { // TODO: delete copy constructor to avoid accidental copying?
    Footprint footprint;
    z3::context context;
    z3::solver solver;
    FootprintEncoder encoder;
    std::deque<std::pair<z3::expr, std::function<void(FootprintEncoding&,bool)>>> checks;

    explicit FootprintEncoding(Footprint&& footprint) : footprint(std::move(footprint)), solver(context), encoder(context) {
        solver.add(EncodeFlow(footprint, encoder));
    }
};

//std::vector<bool> ComputeImplied(FootprintEncoding& encoding, const z3::expr_vector& expressions, const std::string& prefix) {
////    encoding.solver.push();
//
//    // prepare required vectors
//    z3::expr_vector variables(encoding.context);
//    z3::expr_vector assumptions(encoding.context);
//    z3::expr_vector consequences(encoding.context);
//
//    // prepare solver
//    for (std::size_t index = 0; index < expressions.size(); ++index) {
//        std::string name = "__chk_" + prefix + std::to_string(index);
//        auto var = encoding.context.bool_const(name.c_str());
//        variables.push_back(var);
//        encoding.solver.add(var == expressions[index]);
//    }
//
//    // check
//    auto answer = encoding.solver.consequences(assumptions, variables, consequences);
////    encoding.solver.pop();
//
//    // create result
//    std::vector<bool> result(expressions.size(), false);
//    switch (answer) {
//        case z3::unknown:
//            throw std::logic_error("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");
//
//        case z3::unsat:
//            result.flip();
//            return result;
//
//        case z3::sat:
//            // TODO: implement more robust version (add result to solver and then check given exprs?)
//            for (std::size_t index = 0; index < expressions.size(); ++index) {
//                auto searchFor = z3::implies(encoding.context.bool_val(true), variables[index]);
//                for (auto implication : consequences) {
//                    bool syntacticallyEqual = z3::eq(implication, searchFor);
//                    if (!syntacticallyEqual) continue;
//                    result.at(index) = true;
//                    break;
//                }
//            }
//            return result;
//    }
//}

//
// Check that footprint contains all newly published nodes
//

void CheckPublishing(Footprint& footprint) {
    for (auto& node : footprint.nodes) {
        if (node.preLocal == node.postLocal) continue;
        node.needed = true;
        for (auto& field : node.pointerFields) {
            auto next = GetNodeOrNull(footprint, field.postValue);
            if (!next) throw std::logic_error("Footprint too small to capture publishing"); // TODO: better error handling
            next->needed = true;
        }
    }
}

//
// Check that footprint contains all nodes with changed inflow
//

void CheckFlowCoverage(FootprintEncoding& encoding) {
    // find nodes the outflow of which has changed
    auto qv = encoding.encoder.context.int_const("__qv");
    z3::expr_vector equalities(encoding.context);
    std::deque<std::pair<const FootprintNode*, const Field*>> fields;
    for (const auto& node : encoding.footprint.nodes) {
        for (const auto& field : node.pointerFields) {
            auto sameFlow = encoding.encoder(*field.preRootOutflow)(qv) == encoding.encoder(*field.postRootOutflow)(qv);
            equalities.push_back(z3::forall(qv, sameFlow));
            fields.emplace_back(&node, &field);
        }
    }
    auto unchanged = ComputeImplied(encoding, equalities, "flow");
    assert(unchanged.size() == fields.size());

    // find minimal footprint
    std::set<const SymbolicVariableDeclaration*> footprintMustContain;
    footprintMustContain.insert(&encoding.footprint.nodes.front().address);
    for (std::size_t index = 0; index < unchanged.size(); ++index) {
        if (unchanged[index]) continue;
        footprintMustContain.insert(&fields[index].first->address);
        footprintMustContain.insert(&fields[index].second->preValue.get());
        footprintMustContain.insert(&fields[index].second->postValue.get());
    }

    // check that minimal footprint contains changes
    for (auto address : footprintMustContain) {
        auto mustHaveNode = GetNodeOrNull(encoding.footprint, *address);
        if (!mustHaveNode) throw std::logic_error("Footprint too small: does not cover address the inflow of which changed"); // TODO: better error handling
        mustHaveNode->needed = true;
    }
}

//
// Check keyset and inflow uniqueness
//

void CheckFlowUniqueness(FootprintEncoding& encoding) {
    z3::expr_vector checks(encoding.context);
    checks.push_back(EncodeKeysetDisjointness(encoding.footprint, encoding.encoder, Mode::POST));
    checks.push_back(EncodeInflowUniqueness(encoding.footprint, encoding.encoder, Mode::POST));
    auto isSatisfied = ComputeImplied(encoding, checks, "unique");

    if (!isSatisfied[0]) throw std::logic_error("Unsafe update: keysets are not disjoint."); // TODO: better error handling
    if (!isSatisfied[1]) throw std::logic_error("Unsafe update: inflow not unique."); // TODO: better error handling
}

//
// Check invariant
//

void CheckInvariant(FootprintEncoding& encoding) {
    z3::expr_vector checks(encoding.context);
    for (const auto& node : encoding.footprint.nodes) {
        auto nodeInvariant = InstantiateInvariant(encoding.footprint, node, Mode::POST);
        checks.push_back(encoding.encoder(*nodeInvariant));
    }
    auto isSatisfied = ComputeImplied(encoding, checks, "invariant");
    if (std::none_of(isSatisfied.begin(), isSatisfied.end(), std::logical_not<>())) return;
    throw std::logic_error("Unsafe update: invariant is not maintained."); // TODO: better error handling
}

//
// Check specification
//

void CheckSpecification(FootprintEncoding& encoding) {
    z3::expr_vector checks(encoding.context);

}

//
// Post image
//

void MinimizeFootprint(Footprint& footprint) {
    footprint.nodes.erase(std::remove_if(footprint.nodes.begin(), footprint.nodes.end(),
                                         [](const auto& node) { return !node.needed; }), footprint.nodes.end());
}

std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs) const {
    // TODO: use future
    // TODO: create and use update/effect lookup table
    // TODO: inline as much as possible?

    auto [isVar, lhsVar] = heal::IsOfType<VariableExpression>(*lhs.expr);
    if (!isVar) throw std::logic_error("Unsupported assignment: dereference of non-variable"); // TODO: better error handling
    auto [isSimple, rhs] = heal::IsOfType<SimpleExpression>(*cmd.rhs);
    if (!isSimple) throw std::logic_error("Unsupported assignment: right-hand side is not simple"); // TODO:: better error handling

    Footprint footprint = MakeFootprint(std::move(pre), lhsVar->decl, lhs.fieldname, *rhs);
    auto encoding = FootprintEncoding(std::move(footprint));

    CheckPublishing(encoding.footprint);
    CheckFlowCoverage(encoding);
    CheckFlowUniqueness(encoding);
    CheckInvariant(encoding);
    CheckSpecification(encoding);

    MinimizeFootprint(encoding.footprint);
    // TODO: extract effect
    throw std::logic_error("not yet implemented");
}
