#include "default_solver.hpp"

#include "z3++.h"
#include "eval.hpp"
#include "cola/util.hpp" // TODO: delete

using namespace cola;
using namespace heal;
using namespace solver;


static constexpr const bool INVARIANT_FOR_LOCAL_RESOURCES = false; // TODO: REMOVE! should be done by the invariant

enum struct Mode { PRE, POST };

struct Field {
    const std::string name;
    const Type& type;
    std::reference_wrapper<const SymbolicVariableDeclaration> preValue;
    std::reference_wrapper<const SymbolicVariableDeclaration> postValue;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> preAllOutflow;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> preRootOutflow;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> postAllOutflow;
    std::optional<std::reference_wrapper<const SymbolicFlowDeclaration>> postRootOutflow;

    Field(Field&& other) = default;
    Field(const Field& other) = delete;
};

struct FootprintNode {
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

    FootprintNode(FootprintNode&& other) = default;
    FootprintNode(const FootprintNode& other) = delete;
};

struct Footprint {
    const SolverConfig& config;
    std::deque<FootprintNode> nodes;
    SymbolicFactory factory;
    std::unique_ptr<Annotation> pre;

    Footprint(Footprint&& other) = default;
    Footprint(const Footprint& other) = delete;
};

FootprintNode* GetNodeOrNull(Footprint& footprint, const SymbolicVariableDeclaration& address) {
    // search for an existing node at the given address
    auto pred = [&address](const auto& item){ return &item.address == &address; };
    auto find = std::find_if(footprint.nodes.begin(), footprint.nodes.end(), pred);
    if (find != footprint.nodes.end()) return &(*find);
    return nullptr;
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

bool IsRoot(const Footprint& footprint, const FootprintNode& node) {
    return &node == &footprint.nodes.front();
}

std::vector<const Field*> IncomingEdges(Footprint& footprint, const FootprintNode& target, Mode mode) {
    std::vector<const Field*> result;
    result.reserve(footprint.nodes.size() * target.pointerFields.size());

    for (const auto& node : footprint.nodes) {
        for (const auto& field : node.pointerFields) {
            auto nextAddress = mode == Mode::PRE ? field.preValue : field.postValue;
            auto next = GetNodeOrNull(footprint, nextAddress);
            if (!next || next != &target) continue;
            result.push_back(&field);
        }
    }

    return result;
}

struct ObligationFinder : public DefaultLogicListener {
    const ObligationAxiom* result = nullptr;
    void enter(const ObligationAxiom& obl) override { result = &obl; }
};

const ObligationAxiom* GetObligationOrNull(const Footprint& footprint) {
    ObligationFinder finder;
    footprint.pre->now->accept(finder);
    return finder.result;
}

std::unique_ptr<PointsToAxiom> ToLogic(const FootprintNode& node, Mode mode) {
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& field : node.pointerFields) {
        fields[field.name] = std::make_unique<SymbolicVariable>(mode == Mode::PRE ? field.preValue : field.postValue);
    }
    for (const auto& field : node.dataFields) {
        fields[field.name] = std::make_unique<SymbolicVariable>(mode == Mode::PRE ? field.preValue : field.postValue);
    }
    return std::make_unique<PointsToAxiom>(
            std::make_unique<SymbolicVariable>(node.address),
            mode == Mode::PRE ? node.preLocal : node.postLocal,
            mode == Mode::PRE ? node.preAllInflow : node.postAllInflow,
            std::move(fields)
    );
}

//
// Create Footprint, updating local bits and invoking invariant for nodes
//

FootprintNode* TryGetOrCreateNode(Footprint& footprint, LazyValuationMap& eval, const SymbolicVariableDeclaration& nextAddress) {
    // find the memory resource referenced by nextAddress, abort if none exists
    auto* resource = eval.GetMemoryResourceOrNull(nextAddress);
    if (!resource) return nullptr;

    // search for an existing node at the given address, care for aliasing
    auto& nextAlias = resource->node->Decl();
    if (auto find = GetNodeOrNull(footprint, nextAlias)) return find;

    // create a new node
    auto& flowType = resource->flow.get().type;
    auto& preRootInflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& postRootInflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& postAllInflow = footprint.factory.GetUnusedFlowVariable(flowType); // flow might have changed
    auto& preKeyset = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& postKeyset = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    auto& frameInflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
    FootprintNode result{nextAlias, false, resource->isLocal, resource->isLocal, resource->flow, preRootInflow, preKeyset,
                         postAllInflow, postRootInflow, postKeyset, frameInflow, {}, {} };
    result.dataFields.reserve(resource->fieldToValue.size());
    result.pointerFields.reserve(resource->fieldToValue.size());
    for (const auto& [name, value] : resource->fieldToValue) {
        Field field{ name, value->Type(), value->Decl(), value->Decl(), std::nullopt, std::nullopt, std::nullopt, std::nullopt };
        if (value->Sort() == Sort::PTR) {
            field.preAllOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            field.preRootOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            field.postAllOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            field.postRootOutflow = footprint.factory.GetUnusedFlowVariable(flowType); // compute later
            result.pointerFields.push_back(std::move(field));
        } else {
            result.dataFields.push_back(std::move(field));
        }
    }

    // add to footprint
    footprint.nodes.push_back(std::move(result));
    return &footprint.nodes.back();
}

void ExpandFootprint(Footprint& footprint, LazyValuationMap& eval) {
    std::set<std::pair<std::size_t, FootprintNode*>, std::greater<>> worklist;
    worklist.emplace(footprint.config.maxFootprintDepth, &*footprint.nodes.begin());

    while (!worklist.empty()) {
        auto [depth, node] = *worklist.begin();
        worklist.erase(worklist.begin());
        if (depth == 0) continue;

        for (auto& field : node->pointerFields) {
            for (auto nextAddress : { &field.preValue, &field.postValue }) {
                auto* nextNode = TryGetOrCreateNode(footprint, eval, *nextAddress);
                if (!nextNode) continue;
                *nextAddress = nextNode->address; // inline alias
                if (!node->postLocal) nextNode->postLocal = false; // publish node
                worklist.emplace(depth-1, nextNode);
            }
        }
    }
}

Footprint MakeFootprint(const SolverConfig& config, std::unique_ptr<Annotation> pre_,
                        const VariableDeclaration& lhsVar, const std::string& lhsField, const SimpleExpression& rhs) {
    Footprint footprint = { config, {}, SymbolicFactory(*pre_), std::move(pre_) };
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
    FootprintNode* root = TryGetOrCreateNode(footprint, eval, eval.GetValueOrFail(lhsVar));
    if (!root) throw std::logic_error("Cannot initialize footprint."); // TODO: better error handling
    root->needed = true;
    root->preRootInflow = root->preAllInflow;
    root->postRootInflow = root->preAllInflow;
    root->postAllInflow = root->preAllInflow;
    GetFieldOrFail(*root, lhsField).postValue = *rhsEval;

    // generate flow graph
    ExpandFootprint(footprint, eval);

    // cyclic footprints are not supported
    if (!IncomingEdges(footprint, *root, Mode::PRE).empty() || !IncomingEdges(footprint, *root, Mode::POST).empty()) {
        throw std::logic_error("Unsupported update: cyclic footprints are not supported");
    }

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
    explicit FootprintEncoder(z3::context& context) : context(context), result(context) {}
    FootprintEncoder(const FootprintEncoder& other) = delete;

    z3::expr operator()(const LogicObject& obj) { return Encode(obj); }
    z3::expr operator()(const SymbolicVariableDeclaration& decl) { return EncodeSymbol(decl); }
    z3::func_decl operator()(const SymbolicFlowDeclaration& decl) { return EncodeFlow(decl); }
    z3::expr QuantifiedVariable(Sort sort) { return context.constant("__qv", EncodeSort(sort)); }

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
        auto qv = QuantifiedVariable(obj.valueLow->Sort());
        auto premise = (Encode(*obj.valueLow) < qv) && (qv <= Encode(*obj.valueHigh));
        auto conclusion = EncodeFlow(obj.flow)(qv);
        result = z3::forall(qv, z3::implies(premise, conclusion));
    }
    void visit(const InflowEmptinessAxiom& obj) override {
        auto qv = QuantifiedVariable(obj.flow.get().type.sort);
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
    z3::expr result;
    std::map<const SymbolicVariableDeclaration*, z3::expr> symbols;
    std::map<const SymbolicFlowDeclaration*, z3::func_decl> flows;

    template<typename K, typename V, typename F>
    V GetOrCreate(std::map<K, V>& map, K key, F create) {
        auto find = map.find(key);
        if (find != map.end()) return find->second;
        auto newValue = create();
        map.emplace(key, newValue);
        return newValue;
    }

    inline z3::sort EncodeSort(Sort sort) {
        switch (sort) {
            case Sort::VOID: throw std::logic_error("Cannot encode 'VOID' sort."); // TODO: better error handling
            case Sort::BOOL: return context.bool_sort();
            default: return context.int_sort();
        }
    }

    z3::expr EncodeSymbol(const SymbolicVariableDeclaration& decl) {
        return GetOrCreate(symbols, &decl, [this,&decl](){
            auto name = "_v" + decl.name;
            return context.constant(name.c_str(), EncodeSort(decl.type.sort));
        });
    }

    z3::func_decl EncodeFlow(const SymbolicFlowDeclaration& decl) {
        return GetOrCreate(flows, &decl, [this,&decl](){
            auto name = "_V" + decl.name;
            return context.function(name.c_str(), EncodeSort(decl.type.sort), context.bool_sort());
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
        static SymbolicNull nullPtr;
        ResourceCollector collector;
        obj.accept(collector);
        if (collector.resources.empty()) return context.bool_val(true);
        z3::expr_vector vec(context);
        vec.push_back(Encode(nullPtr));
        for (const auto* resource : collector.resources) {
            vec.push_back(EncodeSymbol(*resource));
        }
        return z3::distinct(vec);
    }
};

z3::expr EncodeInvariant(const SolverConfig& config, FootprintEncoder& encoder, const FootprintNode& node, Mode mode) {
    if (!INVARIANT_FOR_LOCAL_RESOURCES && mode == Mode::PRE ? node.preLocal : node.postLocal)
        return encoder.context.bool_val(true); // TODO: should be done by the invariant itself

    auto invariant = config.invariant->Instantiate(*ToLogic(node, mode));
    return encoder(*invariant);
}

z3::expr EncodePredicate(const Predicate& predicate, FootprintEncoder& encoder, const FootprintNode& node, const z3::expr& value, Mode mode) {
    auto& arg = predicate.vars[0];
    auto instance = predicate.Instantiate(*ToLogic(node, mode), arg); // TODO: z3 substitution could do this
    z3::expr_vector replace(encoder.context), with(encoder.context);
    replace.push_back(encoder(arg));
    with.push_back(value);
    return encoder(*instance).substitute(replace, with);
}

z3::expr EncodeOutflow(Footprint& footprint, FootprintEncoder& encoder, const FootprintNode& node, const Field& field, Mode mode) {
    /* Remark:
     * Laminar flow may suggest that the keyset due to frame flow needs not be considered.
     * However, we need to check that node inflow remains unique after update. Ignoring inflow that is due to frame
     * flow on a predecessor might result in erroneous conclusions. Hence, we track root and all outflow.
     * One may want to check whether this is strictly necessary!
     */

    bool forPre = mode == Mode::PRE;
    auto qv = encoder.QuantifiedVariable(footprint.config.flowDomain->GetFlowValueType().sort);
    auto& flowPredicate = footprint.config.flowDomain->GetOutFlowContains(field.name);
    auto mkFlow = [&](auto value){ return EncodePredicate(flowPredicate, encoder, node, value, mode); };
    auto rootIn = encoder(forPre ? node.preRootInflow : node.postRootInflow);
    auto rootOut = encoder(forPre ? field.preRootOutflow.value() : field.postRootOutflow.value());
    auto allIn = encoder(forPre ? node.preAllInflow : node.postAllInflow);
    auto allOut = encoder(forPre ? field.preAllOutflow.value() : field.postAllOutflow.value());
    z3::expr_vector result(encoder.context);

    // outflow
    result.push_back(z3::forall(qv, (rootIn(qv) && mkFlow(qv)) == rootOut(qv)));
    result.push_back(z3::forall(qv, (allIn(qv) && mkFlow(qv)) == allOut(qv)));

    // outflow received by successor
    auto* successor = GetNodeOrNull(footprint, forPre ? field.preValue : field.postValue);
    if (successor) {
        auto nextRootIn = encoder(forPre ? successor->preRootInflow : successor->postRootInflow);
        auto nextAllIn = encoder(forPre ? successor->preAllInflow : successor->postAllInflow);
        result.push_back(z3::forall(qv, z3::implies(rootOut(qv), nextRootIn(qv))));
        result.push_back(z3::forall(qv, z3::implies(allOut(qv), nextAllIn(qv))));
    }

    return z3::mk_and(result);
}

std::vector<const SymbolicFlowDeclaration*> GetRootInflowFromPredecessors(Footprint& footprint, const FootprintNode& node, Mode mode) {
    assert(!IsRoot(footprint, node));
    auto incomingEdges = IncomingEdges(footprint, node, mode);
    bool forPre = mode == Mode::PRE;
    std::vector<const SymbolicFlowDeclaration*> result;
    result.reserve(incomingEdges.size());
    for (auto* field : incomingEdges) {
        result.push_back(forPre ? &field->preRootOutflow.value().get() : &field->postRootOutflow.value().get());
    }
    return result;
}

z3::expr EncodeFlowRules(Footprint& footprint, FootprintEncoder& encoder, const FootprintNode& node) {
    auto qv = encoder.QuantifiedVariable(node.frameInflow.get().type.sort);
    z3::expr_vector result(encoder.context);
    auto addRule = [&qv, &result](auto pre, auto imp) { result.push_back(z3::forall(qv, z3::implies(pre, imp))); };

    z3::expr_vector preOuts(encoder.context);
    z3::expr_vector postOuts(encoder.context);
    for (const auto& field : node.pointerFields) {
        preOuts.push_back(encoder(field.preAllOutflow.value())(qv));
        postOuts.push_back(encoder(field.postAllOutflow.value())(qv));
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

    // redundant
    addRule(preRoot == postRoot, preAll == postAll);

    // keyset is subset of all flow and not outflow
    addRule(preKey, preAll && !preOut);
    addRule(postKey, postAll && !postOut);

    // keyset is inflow minus outflow
    addRule(preAll && !preOut, preKey);
    addRule(postAll && !postOut, postKey);

    // rootInflow is due to predecessors (except in root node)
    if (!IsRoot(footprint, node)) {
        for (auto [theRoot, mode] : { std::make_pair(preRoot, Mode::PRE), std::make_pair(postRoot, Mode::POST) }) {
            auto incomingRootFlow = GetRootInflowFromPredecessors(footprint, node, mode);
            z3::expr_vector inflow(encoder.context);
            for (auto* edge : incomingRootFlow) inflow.push_back(encoder(*edge)(qv));
            addRule(theRoot, z3::mk_or(inflow));
        }
    }

    return mk_and(result);
}

z3::expr EncodeKeysetDisjointness(const Footprint& footprint, FootprintEncoder& encoder, Mode mode) {
    auto qv = encoder.QuantifiedVariable(footprint.config.flowDomain->GetFlowValueType().sort);
    z3::expr_vector keyset(encoder.context);
    for (const auto& node : footprint.nodes) {
        auto& nodeKeyset = mode == Mode::PRE ? node.preKeyset.get() : node.postKeyset.get();
        keyset.push_back(encoder(nodeKeyset)(qv));
    }
    return z3::forall(qv, z3::atmost(keyset, 1));
}

z3::expr EncodeInflowUniqueness(Footprint& footprint, FootprintEncoder& encoder, Mode mode) {
    // uniqueness
    auto qv = encoder.QuantifiedVariable(footprint.config.flowDomain->GetFlowValueType().sort);
    z3::expr_vector result(encoder.context);

    for (const auto& node : footprint.nodes) {
        if (IsRoot(footprint, node)) continue;
        z3::expr_vector inflow(encoder.context);
        inflow.push_back(encoder(node.frameInflow)(qv));
        for (auto* edge : GetRootInflowFromPredecessors(footprint, node, mode)) {
            inflow.push_back(encoder(*edge)(qv));
        }
        result.push_back(z3::forall(qv, z3::atmost(inflow, 1)));
    }

    return z3::mk_and(result);
}

z3::expr EncodeFlow(Footprint& footprint, FootprintEncoder& encoder) {
    z3::expr_vector result(encoder.context);
    result.push_back(encoder(*footprint.pre));
    auto qv = encoder.QuantifiedVariable(footprint.config.flowDomain->GetFlowValueType().sort);

    // same inflow in root before and after update
    auto& root = footprint.nodes.front();
    assert(&root.preAllInflow.get() == &root.preRootInflow.get());
    assert(&root.preAllInflow.get() == &root.postRootInflow.get());
    assert(&root.preAllInflow.get() == &root.postAllInflow.get());
    result.push_back(z3::forall(qv, !encoder(root.frameInflow)(qv)));

    // go over graph and encode flow
    for (const auto& node : footprint.nodes) {
        // node invariant
        result.push_back(EncodeInvariant(footprint.config, encoder, node, Mode::PRE));

        // general flow constraints
        result.push_back(EncodeFlowRules(footprint, encoder, node));

        // outflow
        for (const auto& field : node.pointerFields) {
            result.push_back(EncodeOutflow(footprint, encoder, node, field, Mode::PRE));
            result.push_back(EncodeOutflow(footprint, encoder, node, field, Mode::POST));
        }
    }

    // add assumptions for pre flow: keyset disjointness, inflow uniqueness
    result.push_back(EncodeKeysetDisjointness(footprint, encoder, Mode::PRE));
    result.push_back(EncodeInflowUniqueness(footprint, encoder, Mode::PRE));

    return z3::mk_and(result);
}

struct FootprintEncoding { // TODO: delete copy constructor to avoid accidental copying?
    Footprint footprint;
    z3::context context;
    z3::solver solver;
    FootprintEncoder encoder;

    FootprintEncoding(const FootprintEncoding& other) = delete;
    explicit FootprintEncoding(Footprint&& footprint_) : footprint(std::move(footprint_)), solver(context), encoder(context) {
        solver.add(EncodeFlow(footprint, encoder));
        assert(solver.check() != z3::unsat);
    }
};

//
// Checks
//

struct FootprintChecks { // TODO: delete copy constructor to avoid accidental copying?
    std::deque<std::pair<z3::expr, std::function<void(bool)>>> checks;
    std::deque<std::unique_ptr<SpecificationAxiom>> postSpec;

    void Add(const z3::expr& expr, std::function<void(bool)> callback) {
        checks.emplace_back(expr, std::move(callback));
    }
};

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

bool DebugCheck(FootprintEncoding& encoding, const z3::expr& expr) {
    encoding.solver.push();
    std::cout << "checking: " << expr << std::endl;
    encoding.solver.add(!expr);
    auto res = encoding.solver.check();
    encoding.solver.pop();
    switch (res) {
        case z3::unsat: std::cout << "  ==> holds" << std::endl; return true;
        case z3::sat: std::cout << "  ==> does not hold" << std::endl; return false;
        case z3::unknown: std::cout << "  ==> solving failed" << std::endl; return false;
    }
}

void AddFlowCoverageChecks(FootprintEncoding& encoding, FootprintChecks& checks) {
    auto ensureFootprintContains = [&encoding](const SymbolicVariableDeclaration& address){
        auto mustHaveNode = GetNodeOrNull(encoding.footprint, address);
        if (!mustHaveNode) throw std::logic_error("Footprint too small: does not cover all addresses the inflow of which changed"); // TODO: better error handling
        mustHaveNode->needed = true;
    };

    auto qv = encoding.encoder.QuantifiedVariable(encoding.footprint.config.flowDomain->GetFlowValueType().sort);
    for (const auto& node : encoding.footprint.nodes) {
        for (const auto& field : node.pointerFields) {
            auto sameFlow = encoding.encoder(field.preRootOutflow.value())(qv) == encoding.encoder(field.postRootOutflow.value())(qv);
            checks.Add(z3::forall(qv, sameFlow), [ensureFootprintContains,&node,&field](bool unchanged){
                std::cout << "Checking outflow at " << node.address.name << ": unchanged=" << unchanged << std::endl;
                if (unchanged) return;
                ensureFootprintContains(node.address);
                ensureFootprintContains(field.preValue);
                ensureFootprintContains(field.postValue);
            });
        }
    }
}

void AddFlowUniquenessChecks(FootprintEncoding& encoding, FootprintChecks& checks) {
    auto disjointness = EncodeKeysetDisjointness(encoding.footprint, encoding.encoder, Mode::POST);
    checks.Add(disjointness, [](bool holds){
        std::cout << "Checking keyset disjointness: holds=" << holds << std::endl;
        if (holds) return;
        throw std::logic_error("Unsafe update: keyset disjointness not guaranteed."); // TODO: better error handling
    });

    auto uniqueness = EncodeInflowUniqueness(encoding.footprint, encoding.encoder, Mode::POST);
    checks.Add(uniqueness, [](bool holds){
        std::cout << "Checking inflow uniqueness: holds=" << holds << std::endl;
        if (holds) return;
        throw std::logic_error("Unsafe update: inflow uniqueness not guaranteed."); // TODO: better error handling
    });
}

void AddSpecificationChecks(FootprintEncoding& encoding, FootprintChecks& checks) {
    auto obligation = GetObligationOrNull(encoding.footprint);
    auto qv = encoding.encoder.QuantifiedVariable(encoding.footprint.config.flowDomain->GetFlowValueType().sort);
    auto contains = [&encoding](auto& node, auto var, auto mode) {
        return EncodePredicate(*encoding.footprint.config.logicallyContainsKey, encoding.encoder, node, var, mode);
    };

    // check purity
//    z3::expr_vector purity(encoding.encoder.context);
//    for (const auto& node : encoding.footprint.nodes) {
//        purity.push_back(z3::forall(qv, encoding.encoder(node.preKeyset)(qv) == encoding.encoder(node.postKeyset)(qv)));
//    }
    z3::expr_vector preContains(encoding.encoder.context);
    z3::expr_vector postContains(encoding.encoder.context);
    for (const auto& node : encoding.footprint.nodes) {
        preContains.push_back(encoding.encoder(node.preKeyset)(qv) && contains(node, qv, Mode::PRE));
        postContains.push_back(encoding.encoder(node.postKeyset)(qv) && contains(node, qv, Mode::POST));
    }
    auto isPure = z3::forall(qv, z3::mk_or(preContains) == z3::mk_or(postContains));
    checks.Add(isPure, [obligation,&checks](bool isPure){
        std::cout << "Checking purity: isPure=" << isPure << std::endl;
        if (!isPure && !obligation) throw std::logic_error("Unsafe update: impure update without obligation."); // TODO: better error handling
        if (isPure && obligation) checks.postSpec.push_back(heal::Copy(*obligation));
    });

    // impure updates require obligation, checked together with purity
    if (!obligation) return;
    if (obligation->kind == SpecificationAxiom::Kind::CONTAINS) return;
    auto key = encoding.encoder(*obligation->key);

    // check impure
    z3::expr_vector othersPreContained(encoding.encoder.context);
    z3::expr_vector othersPostContained(encoding.encoder.context);
    z3::expr_vector keyPreContained(encoding.encoder.context);
    z3::expr_vector keyPostContained(encoding.encoder.context);
    for (const auto& node : encoding.footprint.nodes) {
        auto preKeys = encoding.encoder(node.preKeyset);
        auto postKeys = encoding.encoder(node.postKeyset);
        othersPreContained.push_back(preKeys(qv) && contains(node, qv, Mode::PRE));
        othersPostContained.push_back(postKeys(qv) && contains(node, qv, Mode::POST));
        keyPreContained.push_back(preKeys(key) && contains(node, key, Mode::PRE));
        keyPostContained.push_back(postKeys(key) && contains(node, key, Mode::POST));
    }
    auto othersUnchanged = z3::forall(qv, z3::implies(qv != key, z3::mk_or(othersPreContained) == z3::mk_or(othersPostContained)));
    auto isInsertion = !z3::mk_or(keyPreContained) && z3::mk_or(keyPostContained) && othersUnchanged;
    auto isDeletion = z3::mk_or(keyPreContained) && !z3::mk_or(keyPostContained) && othersUnchanged;
    auto isTarget = obligation->kind == SpecificationAxiom::Kind::INSERT ? isInsertion : isDeletion;
    
    checks.Add(isTarget, [&checks,obligation](bool isTarget){
        std::cout << "Checking impure specification wrt obligation: holds=" << isTarget << std::endl;
        if (isTarget) {
            checks.postSpec.push_back(std::make_unique<FulfillmentAxiom>(obligation->kind, heal::Copy(*obligation->key), true));
        } else if (checks.postSpec.empty()) {
            // if the update was pure, then there was an obligation in checks.postSpec
            // hence, the update must be impure and it does not satisfy the spec
            throw std::logic_error("Unsafe update: impure update that does not satisfy the specification");
        }
    });
}

void AddInvariantChecks(FootprintEncoding& encoding, FootprintChecks& checks) {
    for (const auto& node : encoding.footprint.nodes) {
        auto nodeInvariant = EncodeInvariant(encoding.footprint.config, encoding.encoder, node, Mode::POST);
        checks.Add(nodeInvariant, [&node](bool holds){
            std::cout << "Checking invariant for " << node.address.name << ": holds=" << holds << std::endl;
            if (holds) return;
            throw std::logic_error("Unsafe update: invariant is not maintained."); // TODO: better error handling
        });
    }
}

//
// Post image
//

//std::vector<bool> ComputeImplied(FootprintEncoding& encoding, const z3::expr_vector& expressions) {
//    // prepare required vectors
//    z3::expr_vector variables(encoding.context);
//    z3::expr_vector assumptions(encoding.context);
//    z3::expr_vector consequences(encoding.context);
//
//    // prepare solver
//    for (unsigned int index = 0; index < expressions.size(); ++index) {
//        std::string name = "__chk" + std::to_string(index);
//        auto var = encoding.context.bool_const(name.c_str());
//        variables.push_back(var);
//        encoding.solver.add(var == expressions[(int) index]);
//    }
//
//    // check
//    auto answer = encoding.solver.consequences(assumptions, variables, consequences);
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
//            // TODO: implement more robust version (add result to solver and then check given expressions?)
//            for (unsigned int index = 0; index < expressions.size(); ++index) {
//                auto searchFor = z3::implies(encoding.context.bool_val(true), variables[(int) index]);
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

std::vector<bool> ComputeImplied(FootprintEncoding& encoding, const z3::expr_vector& expressions) {
    // TODO: solve all expressions in one shot
    std::vector<bool> result;

    for (const auto& expr : expressions) {
        encoding.solver.push();
        encoding.solver.add(!expr);
        auto res = encoding.solver.check();
        encoding.solver.pop();
        bool chk;
        switch (res) {
            case z3::unsat: chk = true; break;
            case z3::sat: chk = false; break;
            case z3::unknown: throw std::logic_error("Solving failed."); // TODO: better error handling
        }
        if (chk) encoding.solver.add(expr); // TODO: really do this?
        result.push_back(chk);
    }

    return result;
}

void PerformChecks(FootprintEncoding& encoding, const FootprintChecks& checks) {
    z3::expr_vector expressions(encoding.context);
    for (const auto& check : checks.checks) {
        expressions.push_back(check.first);
    }
    auto implied = ComputeImplied(encoding, expressions);
    assert(checks.checks.size() == implied.size());
    for (std::size_t index = 0; index < implied.size(); ++index) {
        checks.checks.at(index).second(implied.at(index));
    }
}

void MinimizeFootprint(Footprint& footprint) {
//    footprint.nodes.erase(std::remove_if(footprint.nodes.begin(), footprint.nodes.end(),
//                                         [](const auto& node) { return !node.needed; }), footprint.nodes.end());
    std::deque<FootprintNode> nodes;
    for (auto& node : footprint.nodes) {
        if (!node.needed) continue;
        nodes.push_back(std::move(node));
    }
    footprint.nodes = std::move(nodes);
}

std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs) const {
    // TODO: use future
    // TODO: create and use update/effect lookup table
    // TODO: inline as much as possible?
    // TODO: start with smaller footprint and increase if too small?

    // debug
    std::cout << std::endl << std::endl << std::endl << "====================" << std::endl;
    unsigned int major, minor, build, revision;
    Z3_get_version(&major, &minor, &build, &revision);
    std::cout << "Using Z3 version: " << major << "." << minor << "." << build << "." << revision << std::endl;
    std::cout << "Command: "; cola::print(cmd, std::cout);
    std::cout << "Pre: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl << std::flush;
    // end debug

    auto [isVar, lhsVar] = heal::IsOfType<VariableExpression>(*lhs.expr);
    if (!isVar) throw std::logic_error("Unsupported assignment: dereference of non-variable"); // TODO: better error handling
    auto [isSimple, rhs] = heal::IsOfType<SimpleExpression>(*cmd.rhs);
    if (!isSimple) throw std::logic_error("Unsupported assignment: right-hand side is not simple"); // TODO:: better error handling

    Footprint footprint = MakeFootprint(Config(), std::move(pre), lhsVar->decl, lhs.fieldname, *rhs);
    std::cout << "Footprint size = " << footprint.nodes.size() << std::endl;
    for (auto& node : footprint.nodes) {
        std::cout << "Node pre:  ";
        heal::Print(*ToLogic(node, Mode::PRE), std::cout);
        std::cout << std::endl;
        std::cout << "Node post: ";
        heal::Print(*ToLogic(node, Mode::POST), std::cout);
        std::cout << std::endl;
    }
    FootprintEncoding encoding(std::move(footprint));

    FootprintChecks checks;
    AddFlowCoverageChecks(encoding, checks);
    AddFlowUniquenessChecks(encoding, checks);
    AddSpecificationChecks(encoding, checks);
    AddInvariantChecks(encoding, checks);

    CheckPublishing(encoding.footprint);
    PerformChecks(encoding, checks);
    throw std::logic_error("--- breakpoint ---");

    MinimizeFootprint(encoding.footprint);
    // TODO: extract effect
    throw std::logic_error("not yet implemented");
}
