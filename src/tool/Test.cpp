#include <chrono>
#include <iomanip>
#include "cola/ast.hpp"
#include "cola/util.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"
#include "heal/properties.hpp"
#include "heal/flow.hpp"
#include "solver/config.hpp"
#include "solver/solver.hpp"

#include "prover/verify.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

constexpr std::size_t FOOTPRINT_DEPTH = 1;
constexpr bool INVARIANT_FOR_LOCAL_RESOURCES = false;

SymbolicFactory factory;

std::unique_ptr<PointsToAxiom> MakePointsTo(const Type& nodeType, const Type& flowType, bool isLocal=false) {
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& [name, type] : nodeType.fields) {
        fields.emplace(name, std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type)));
    }
    auto node = std::make_unique<PointsToAxiom>(
            std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(nodeType)), isLocal,
            factory.GetUnusedFlowVariable(flowType), std::move(fields)
    );
    return node;
}

std::unique_ptr<PointsToAxiom> MakePointsTo(const SolverConfig& config, bool isLocal=false) {
    return MakePointsTo(config.flowDomain->GetNodeType(), config.flowDomain->GetFlowValueType(), isLocal);
}


//
// SOLVER
//

template<typename F>
std::unique_ptr<Predicate> MakePredicate(const Type& nodeType, const Type& flowType, std::string name, F makeBlueprint) {
    // node
    auto node = MakePointsTo(nodeType, flowType);

    // args
    auto& arg = factory.GetUnusedSymbolicVariable(flowType);
    std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, 1> vars = { arg };

    // blueprint
    auto blueprint = makeBlueprint(*node, vars);

    return std::make_unique<Predicate>(name, std::move(node), vars, std::move(blueprint));
}

std::unique_ptr<Invariant> MakeInvariant(const Type& nodeType, const Type& flowType) {
    auto dummy = MakePredicate(nodeType, flowType, "invariant", [](auto& node, auto&){
        auto blueprint = std::make_unique<SeparatingConjunction>();
        auto isUnmarked = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at("mark")->Decl()),
                SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(false)
        );
        auto dataInInflow = std::make_unique<InflowContainsValueAxiom>(
                node.flow, heal::Copy(*node.fieldToValue.at("data"))
        );
        blueprint->conjuncts.push_back(std::make_unique<SeparatingImplication>(
                std::move(isUnmarked), std::move(dataInInflow)
        ));
        auto isNotTail = std::make_unique<SymbolicAxiom>( // assume node is global
                std::make_unique<SymbolicVariable>(node.fieldToValue.at("data")->Decl()),
                SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
        );
        auto nextNotNull = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at("next")->Decl()),
                SymbolicAxiom::NEQ, std::make_unique<SymbolicNull>()
        );
        blueprint->conjuncts.push_back(std::make_unique<SeparatingImplication>(
                std::move(isNotTail), std::move(nextNotNull)
        ));

        return blueprint;
    });
    std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, 0> vars = {};
    return std::make_unique<Invariant>(dummy->name, std::move(dummy->resource), vars, std::move(dummy->blueprint));
}

std::unique_ptr<Predicate> MakeContainsPredicate(const Type& nodeType, const Type& flowType) {
    return MakePredicate(nodeType, flowType, "contains", [](auto& node, auto& vars){
        auto unmarked = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at("mark")->Decl()),
                SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(false)
        );
        auto data = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at("data")->Decl()),
                SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(vars[0])
        );
        auto blueprint = std::make_unique<SeparatingConjunction>();
        blueprint->conjuncts.push_back(std::move(unmarked));
        blueprint->conjuncts.push_back(std::move(data));
        return blueprint;
    });
}

std::unique_ptr<Type> MakeNodeType() {
    auto result = std::make_unique<Type>("Node", Sort::PTR);
    result->fields.emplace("data", Type::data_type());
    result->fields.emplace("mark", Type::bool_type());
    result->fields.emplace("next", *result);
    return result;
}

std::unique_ptr<Predicate> MakeOutflowPredicate(const Type& nodeType, const Type& flowType) {
    return MakePredicate(nodeType, flowType, "outflow", [](auto& node, auto& vars){
        auto marked = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at("mark")->Decl()),
                SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(true)
        );
        auto unmarked = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at("data")->Decl()),
                SymbolicAxiom::LT, std::make_unique<SymbolicVariable>(vars[0])
        );
        auto disjunction = std::make_unique<StackDisjunction>();
        disjunction->axioms.push_back(std::move(marked));
        disjunction->axioms.push_back(std::move(unmarked));
        auto blueprint = std::make_unique<SeparatingConjunction>();
        blueprint->conjuncts.push_back(std::move(disjunction));
        return blueprint;
    });
}

struct TestFlowDomain : public FlowDomain {
    std::unique_ptr<Type> nodeType;
    std::unique_ptr<Predicate> outflow;

    TestFlowDomain() : nodeType(MakeNodeType()), outflow(MakeOutflowPredicate(*nodeType, Type::data_type())) {}

    [[nodiscard]] const cola::Type& GetNodeType() const override { return *nodeType; }
    [[nodiscard]] const cola::Type& GetFlowValueType() const override { return Type::data_type(); }
    [[nodiscard]] const Predicate& GetOutFlowContains(std::string fieldName) const override { return *outflow; }
};

std::shared_ptr<SolverConfig> MakeConfig() {
    auto domain = std::make_unique<TestFlowDomain>();
    auto contains = MakeContainsPredicate(domain->GetNodeType(), domain->GetFlowValueType());
    auto invariant = MakeInvariant(domain->GetNodeType(), domain->GetFlowValueType());
    return std::make_shared<SolverConfig>(FOOTPRINT_DEPTH, std::move(domain), std::move(contains), std::move(invariant), INVARIANT_FOR_LOCAL_RESOURCES);
}

//
// TESTS
//

struct VarStore {
    VariableDeclaration key;
    VariableDeclaration lhs;
    VariableDeclaration rhs;

    explicit VarStore(const Type& nodeType) : key("key", Type::data_type(), false),
                                              lhs("left", nodeType, false),
                                              rhs("right", nodeType, false) {}
};

void TestUnlinkNode(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto mid = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());
    auto past = MakePointsTo(*solver.GetConfig());

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> mid -> end -> past
    mid->node->decl_storage = root->fieldToValue["next"]->Decl();
    end->node->decl_storage = mid->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(right->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(end->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(true)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
        ObligationAxiom::Kind::INSERT, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(past));
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(mid));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    // test
    Assignment command(std::make_unique<Dereference>(std::make_unique<VariableExpression>(store.lhs), "next"), std::make_unique<VariableExpression>(store.rhs));
    solver.Post(std::move(annotation), command);
}

void TestInsertNode(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());
    auto past = MakePointsTo(*solver.GetConfig());
    auto owned = MakePointsTo(*solver.GetConfig(), true);

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> end -> past, owned -> end
    end->node->decl_storage = root->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();
    owned->fieldToValue["next"]->decl_storage = end->node->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(right->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(owned->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(owned->fieldToValue["data"]->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::GT,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(owned->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<InflowEmptinessAxiom>(owned->flow, true));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::INSERT, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<InflowContainsValueAxiom>(root->flow, std::make_unique<SymbolicVariable>(key->value->Decl())));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(past));
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(owned));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    // test
    Assignment command(std::make_unique<Dereference>(std::make_unique<VariableExpression>(store.lhs), "next"), std::make_unique<VariableExpression>(store.rhs));
    solver.Post(std::move(annotation), command);
}

void TestMarkNode(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());
    auto past = MakePointsTo(*solver.GetConfig());

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> end -> past
    end->node->decl_storage = root->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::DELETE, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<InflowContainsValueAxiom>(root->flow, std::make_unique<SymbolicVariable>(key->value->Decl())));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(past));
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    // test
    Assignment command(std::make_unique<Dereference>(std::make_unique<VariableExpression>(store.lhs), "mark"), std::make_unique<BooleanValue>(true));
    solver.Post(std::move(annotation), command);
}

void TestReadNext(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());
    auto past = MakePointsTo(*solver.GetConfig());

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> end -> past
    end->node->decl_storage = root->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::GT,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::DELETE, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<InflowContainsValueAxiom>(root->flow, std::make_unique<SymbolicVariable>(key->value->Decl())));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(past));
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    // test
    Assignment command(std::make_unique<VariableExpression>(store.rhs), std::make_unique<Dereference>(std::make_unique<VariableExpression>(store.lhs), "next"));
    solver.Post(std::move(annotation), command);
}

void TestReadExpand(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> end -> past
    end->node->decl_storage = root->fieldToValue["next"]->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(end->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::DELETE, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<InflowContainsValueAxiom>(root->flow, std::make_unique<SymbolicVariable>(key->value->Decl())));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    // test
    Assignment command(std::make_unique<VariableExpression>(store.rhs), std::make_unique<Dereference>(std::make_unique<VariableExpression>(store.lhs), "next"));
    solver.Post(std::move(annotation), command);
}

void TestReadExpandLocal(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());
    auto owned = MakePointsTo(*solver.GetConfig(), true);

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> end -> past
    end->node->decl_storage = root->fieldToValue["next"]->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(end->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::DELETE, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<InflowContainsValueAxiom>(root->flow, std::make_unique<SymbolicVariable>(key->value->Decl())));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(owned));
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    // test
    Assignment command(std::make_unique<VariableExpression>(store.rhs), std::make_unique<Dereference>(std::make_unique<VariableExpression>(store.lhs), "next"));
    solver.Post(std::move(annotation), command);
}

void TestJoin1(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto mid = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());
    auto past = MakePointsTo(*solver.GetConfig());

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> mid -> end -> past
    mid->node->decl_storage = root->fieldToValue["next"]->Decl();
    end->node->decl_storage = mid->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(mid->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(right->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(end->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(true)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::INSERT, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(past));
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(mid));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    std::vector<std::unique_ptr<Annotation>> toJoin;
    toJoin.push_back(heal::Copy(*annotation));
    toJoin.push_back(heal::Copy(*annotation));
    toJoin.push_back(std::move(annotation));

    solver.Join(std::move(toJoin));
}

void TestJoin2(const Solver& solver) {
    VarStore store(solver.GetConfig()->flowDomain->GetNodeType());
    auto annotation = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(*solver.GetConfig());
    auto mid = MakePointsTo(*solver.GetConfig());
    auto end = MakePointsTo(*solver.GetConfig());
    auto past = MakePointsTo(*solver.GetConfig());

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(store.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(store.key.type)));

    // heap layout: root -> mid -> end -> past
    mid->node->decl_storage = root->fieldToValue["next"]->Decl();
    end->node->decl_storage = mid->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();

    // stack
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(right->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(end->node->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(true)
    ));
    annotation->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    annotation->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::INSERT, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));

    // pushing resources
    annotation->now->conjuncts.push_front(std::move(past));
    annotation->now->conjuncts.push_front(std::move(end));
    annotation->now->conjuncts.push_front(std::move(mid));
    annotation->now->conjuncts.push_front(std::move(root));
    annotation->now->conjuncts.push_front(std::move(right));
    annotation->now->conjuncts.push_front(std::move(left));
    annotation->now->conjuncts.push_front(std::move(key));

    std::vector<std::unique_ptr<Annotation>> toJoin;
    toJoin.push_back(heal::Copy(*annotation));
    toJoin.push_back(heal::Copy(*annotation));
    toJoin.push_back(std::move(annotation));

    solver.Join(std::move(toJoin));
}

//
// MAIN
//

int main(int argc, char** argv) {
    std::size_t testRuns = 1;
    std::vector<std::pair<std::function<void(const Solver&)>, std::string>> tests = {
            { TestUnlinkNode, "Unlink Node" },
            { TestInsertNode, "Insert Node" },
            { TestMarkNode, "Mark Node" },
            { TestReadNext, "Dereference node (fulfilling obligation)" },
            { TestReadExpand, "Expand Frontier (no local nodes)" },
            { TestReadExpandLocal, "Expand Frontier (with local nodes)" },
            { TestJoin1, "Join" },
            { TestJoin2, "Join" },
    };

    auto config = MakeConfig();
    auto solver = solver::MakeDefaultSolver(config);

    std::vector<std::size_t> times;
    times.reserve(tests.size());
    for (const auto& test : tests) {
        auto start = std::chrono::steady_clock::now();
        for (std::size_t count = 0; count < testRuns; ++count) {
            test.first(*solver);
        }
        auto end = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() / testRuns);
    }

    std::cout << std::endl << "Finished " << tests.size() << " tests, run " << testRuns << " times each:" << std::endl;
    auto width = (int) std::max_element(tests.begin(), tests.end(), [](const auto& elem, const auto& other){ return elem.second.length() < other.second.length(); })->second.length();
    for (std::size_t index = 0; index < tests.size(); ++index) {
        std::string name = "'" + tests[index].second + "'";
        std::cout << "  - Test\t" << std::setw(width+2) << name << ":\t avg. " << times[index] << "ms" << std::endl;
    }
}
