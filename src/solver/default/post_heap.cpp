#include "default_solver.hpp"

#include "z3++.h"
#include "eval.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

constexpr const std::size_t FOOTPRINT_DEPTH = 1; // TODO: value should come from flow/config


struct BaseField {
    virtual ~BaseField() = default;
    const std::string name;
    const Type& type;
    std::reference_wrapper<const SymbolicVariableDeclaration> preValue;
    std::reference_wrapper<const SymbolicVariableDeclaration> postValue;

    BaseField(std::string name, const Type& type, const SymbolicVariableDeclaration& preValue, const SymbolicVariableDeclaration& postValue)
            : name(std::move(name)), type(type), preValue(preValue), postValue(postValue) {}
};

struct DataField final : public BaseField {
    DataField(std::string name, const Type& type, const SymbolicVariableDeclaration& preValue, const SymbolicVariableDeclaration& postValue)
            : BaseField(std::move(name), type, preValue, postValue) { assert(type.sort != Sort::PTR); }
};

struct PointerField final : public BaseField {
    std::reference_wrapper<const SymbolicFlowDeclaration> preRootOutflow;
    std::reference_wrapper<const SymbolicFlowDeclaration> postRootOutflow;

    PointerField(std::string name, const Type& type, const SymbolicVariableDeclaration& preValue, const SymbolicVariableDeclaration& postValue,
                 const SymbolicFlowDeclaration& preRootOutflow, const SymbolicFlowDeclaration& postRootOutflow)
            : BaseField(std::move(name), type, preValue, postValue), preRootOutflow(preRootOutflow), postRootOutflow(postRootOutflow) {
        assert(type.sort != Sort::PTR);
    }
};

//struct FieldInfo {
//    std::string name;
//    const Type& type;
//    std::reference_wrapper<const SymbolicVariableDeclaration> preValue;
//    std::reference_wrapper<const SymbolicVariableDeclaration> postValue;
//    std::reference_wrapper<const SymbolicFlowDeclaration> preRootOutflow;
//    std::reference_wrapper<const SymbolicFlowDeclaration> postRootOutflow;
//};

struct FootprintNode {
    const SymbolicVariableDeclaration& address;
    bool preLocal;
    bool postLocal;
    std::reference_wrapper<const SymbolicFlowDeclaration> preInflowFlow;
    std::reference_wrapper<const SymbolicFlowDeclaration> postInflowFlow;
    std::map<std::string, FieldInfo> fields;
};

struct Footprint {
    std::deque<FootprintNode> nodes;
    std::unique_ptr<Annotation> pre;
    SymbolicFactory factory;
    LazyValuationMap eval;
};

std::unique_ptr<Formula> InstantiateInvariant(const Footprint& footprint, const FootprintNode& node, bool forPre) {
    throw std::logic_error("not yet implemented");
}

//
// Create Footprint, updating local bits and invoking invariant for nodes
//

FootprintNode* GetNodeOrNull(Footprint& footprint, const SymbolicVariableDeclaration& address) {
    // search for an existing node at the given address
    auto pred = [&address](const auto& item){ item.address == address; };
    auto find = std::find_if(footprint.nodes.begin(), footprint.nodes.end(), pred);
    if (find != footprint.nodes.end()) return &(*find);
    return nullptr;
}

FootprintNode* GetOrCreateNode(Footprint& footprint, const SymbolicVariableDeclaration& nextAddress) {
    // search for an existing node at the given address
    if (auto find = GetNodeOrNull(footprint, nextAddress)) return find;

    // abort if we do not have the resource for expanding the footprint
    auto* resource = footprint.eval.GetMemoryResourceOrNull(nextAddress);
    if (!resource) return nullptr;

    // create a new node
    auto& postAllFlow = footprint.factory.GetUnusedFlowVariable(resource->flow.get().type); // flow might have changed
    auto& preRootFlow = footprint.factory.GetUnusedFlowVariable(resource->flow.get().type); // computed later
    auto& postRootFlow = footprint.factory.GetUnusedFlowVariable(resource->flow.get().type); // computed later
    std::map<std::string, FieldInfo> fields; // fields are unchanged
    for (const auto& [name, value] : resource->fieldToValue) {
        fields.insert({ name, FieldInfo{ name, value->Type(), value->Decl(), value->Decl() } });
    }
    footprint.nodes.push_back({ nextAddress, resource->isLocal, resource->isLocal, resource->flow, postAllFlow, preRootFlow, postRootFlow, std::move(fields) });
    footprint.pre->now->conjuncts.push_back(InstantiateInvariant(footprint, footprint.nodes.back(), true));
    return &footprint.nodes.back();
}

void ExpandFootprint(Footprint& footprint, FootprintNode& node, std::size_t depth) {
    if (depth == 0) return;
    for (auto& elem : node.fields) {
        if (elem.second.type.sort != Sort::PTR) continue;
        auto descend = [&](const auto& nextAddress) {
            auto* nextNode = GetOrCreateNode(footprint, nextAddress);
            if (!nextNode) return;
            if (!node.postLocal) nextNode->postLocal = false; // publish node
            ExpandFootprint(footprint, *nextNode, depth-1);
        };
        descend(elem.second.preValue);
        descend(elem.second.postValue);
    }
}

void ExpandFootprint(Footprint& footprint, std::size_t depth) {
    ExpandFootprint(footprint, footprint.nodes.front(), depth);
}

Footprint MakeFootprint(std::unique_ptr<Annotation> pre, const VariableDeclaration& lhsVar, std::string lhsField, const SimpleExpression& rhs) {
    LazyValuationMap eval(*pre);
    SymbolicFactory factory(*pre);

    // get variable for rhs value
    const SymbolicVariableDeclaration* rhsEval;
    if (auto var = dynamic_cast<const VariableExpression*>(&rhs)) {
        rhsEval = &eval.GetValueOrFail(var->decl);
    } else {
        auto rhsValue = eval.Evaluate(rhs);
        rhsEval = &factory.GetUnusedSymbolicVariable(rhs.type());
        auto axiom = std::make_unique<SymbolicAxiom>(std::move(rhsValue), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(*rhsEval));
        pre->now->conjuncts.push_back(std::move(axiom));
    }

    // create footprint
    Footprint footprint = { {}, std::move(pre), std::move(factory), std::move(eval) };

    // make root node (update lhsField, do not change flow)
    FootprintNode* root = GetOrCreateNode(footprint, footprint.eval.GetValueOrFail(lhsVar));
    if (!root) throw std::logic_error("Cannot initialize footprint."); // TODO: better error handling
    root->preRootFlow = root->preAllFlow;
    root->postAllFlow = root->preAllFlow;
    root->postRootFlow = root->preAllFlow;
    root->fields.at(lhsField).postValue = *rhsEval;

    // generate flow graph
    ExpandFootprint(footprint, FOOTPRINT_DEPTH); // TODO: start with smaller footprint and increase if too small?
    return footprint;
}

//
// Check that footprint contains all newly published nodes
//

void CheckPublishing(Footprint& footprint) {
    for (auto& node : footprint.nodes) {
        if (node.preLocal == node.postLocal) continue;
        for (auto& elem : node.fields) {
            if (elem.second.type.sort != Sort::PTR) continue;
            auto next = GetNodeOrNull(footprint, elem.second.postValue);
            if (!next) throw std::logic_error("Footprint too small to capture publishing"); // TODO: better error handling
        }
    }
}

//
// Check that footprint contains all nodes with changed inflow
// TODO: make footprint more concise by removing nodes the inflow of which did not change (except root)
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
        auto conclusion = EncodeFlow(obj.flow)(qv) == context.bool_val(true);
        result = z3::forall(qv, z3::implies(premise, conclusion));
    }
    void visit(const InflowEmptinessAxiom& obj) override {
        auto qv = context.int_const("__qv");
        result = z3::exists(qv, EncodeFlow(obj.flow)(qv) == context.bool_val(true));
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

z3::expr EncodeOutFlow(const Footprint& footprint, FootprintEncoder& encoder, const FootprintNode& node, const FieldInfo& field, bool forPre) {
//    successor inflow contains x if node sends x via field

}

z3::expr EncodeFlow(const Footprint& footprint, FootprintEncoder& encoder) {
    z3::expr_vector result(encoder.context);

    // same inflow in root before and after update
    auto& root = footprint.nodes.front();
    assert(&root.preAllFlow.get() == &root.preRootFlow.get());
    assert(&root.preAllFlow.get() == &root.postRootFlow.get());
    assert(&root.preAllFlow.get() == &root.postAllFlow.get());

    // go over graph and encode flow
    for (const auto& node : footprint.nodes) {
        for (const auto& [name, field] : node.fields) {
            if (field.type.sort != Sort::PTR) continue;
            result.push_back(EncodeOutFlow(footprint, encoder, node, field, true));
            result.push_back(EncodeOutFlow(footprint, encoder, node, field, false));
            //
            // successor inflow contains x if node sends x via field
            // Eigentlich will man hier nicht den echten Flow betrachten, sondern nur den der aus der Wurzel stammt.
            // Dazu sollte der Encoder bzw. der Frame keine Infos über den Flow produzieren.
            // Die Flow Information aus footprint.pre benutzen/brauchen wir erst später.
            //
            throw std::logic_error("not yet implemented");
        }
    }

    return z3::mk_and(result);
}

void CheckFlowConstraints(Footprint& footprint) {
    z3::context context;
    z3::solver solver(context);
    FootprintEncoder encoder(context);

    auto frame = encoder.Encode(*footprint.pre, Mode::PRE);
    auto flowPre = EncodeFlow(footprint, encoder);
}

// Algo: (1) create footprint, (2) check local bit update, (3) check unchanged outflow, (4) check invariant, (5) check spec/pure
// at (3): remove nodes the inflow of which did not change
// at (3): check keyset uniqueness and inflow uniqueness





template<typename T, typename R>
const T& As(const R& obj) {
    auto [is, cast] = heal::IsOfType<T>(obj);
    if (!is) throw std::logic_error("Unsupported assignment"); // TODO: better error handling
    return *cast;
}

struct Info {
    const Assignment& assignment;
    const Dereference& lhs;
    const VariableDeclaration& lhsVar;
    const SimpleExpression& rhs;

    std::unique_ptr<SeparatingConjunction> pre;
    std::unique_ptr<SeparatingConjunction> frame;
    std::deque<std::unique_ptr<TimePredicate>> timePredicates;

    std::unique_ptr<SymbolicExpression> currentValue;
    std::unique_ptr<SymbolicExpression> newValue;

    Info(std::unique_ptr<Annotation> pre_, const Assignment &cmd, const Dereference &lhs);

//    void AddPre(std::unique_ptr<Formula> formula) {
//        pre->conjuncts.push_back(std::move(formula));
//    }

//    void UnframeKnowledge(const VariableDeclaration& decl) {
//        throw std::logic_error("not yet implemented");
//    }

    const EqualsToAxiom& UnframeVariableResource(const VariableDeclaration& decl);
    const PointsToAxiom& UnframeMemoryResource(const SymbolicVariableDeclaration& decl);
    void UnframeKnowledge(const SymbolicVariableDeclaration& decl);
};

Info::Info(std::unique_ptr<Annotation> pre_, const Assignment &cmd, const Dereference &lhs)
        : assignment(cmd), lhs(lhs), lhsVar(As<VariableExpression>(lhs).decl), rhs(As<SimpleExpression>(*cmd.rhs)),
          pre(std::make_unique<SeparatingConjunction>()), frame(std::move(pre_->now)), timePredicates(std::move(pre_->time)) {

    // get resources for left-hand side and remove them from the frame
    auto& variableResource = UnframeVariableResource(lhsVar); // dereferenced variable
    auto& memoryResource = UnframeMemoryResource(variableResource.value->Decl()); // accessed memory

    // get resources for right-hand side and remove them from the frame
    if (auto rhsVar = dynamic_cast<const VariableExpression*>(&rhs)) {
        UnframeVariableResource(rhsVar->decl);
    }
}

const EqualsToAxiom& Info::UnframeVariableResource(const VariableDeclaration& decl) {
    throw std::logic_error("not yet implemented"); // TODO: reuse/rewrite eval.hpp
}

const PointsToAxiom& Info::UnframeMemoryResource(const SymbolicVariableDeclaration& decl) {
    throw std::logic_error("not yet implemented"); // TODO: reuse/rewrite eval.hpp
}

void Info::UnframeKnowledge(const SymbolicVariableDeclaration& decl) {
    throw std::logic_error("not yet implemented"); // TODO: collect everything that narrows down decl
}


std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs) const {
    // TODO: use future
    // TODO: create and use update/effect lookup table
    // TODO: inline as much as possible?

    Info info(std::move(pre), cmd, lhs);


    // TODO: implement
    throw std::logic_error("not yet implemented");


}
