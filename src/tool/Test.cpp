#include <chrono>
#include "cola/ast.hpp"
#include "cola/util.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"
#include "heal/properties.hpp"
#include "heal/flow.hpp"
#include "solver/config.hpp"
#include "solver/solver.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

constexpr std::size_t FOOTPRINT_DEPTH = 1;
constexpr bool INVARIANT_FOR_LOCAL_RESOURCES = 0;

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

    const cola::Type& GetNodeType() const override { return *nodeType; }
    const cola::Type& GetFlowValueType() const override { return Type::data_type(); }
    const Predicate& GetOutFlowContains(std::string fieldName) const override { return *outflow; }
};

std::shared_ptr<SolverConfig> MakeConfig() {
    auto domain = std::make_unique<TestFlowDomain>();
    auto contains = MakeContainsPredicate(domain->GetNodeType(), domain->GetFlowValueType());
    auto invariant = MakeInvariant(domain->GetNodeType(), domain->GetFlowValueType());
    return std::make_shared<SolverConfig>(FOOTPRINT_DEPTH, std::move(domain), std::move(contains), std::move(invariant), INVARIANT_FOR_LOCAL_RESOURCES);
}

//
// PRE
//

struct Assign {
    VariableDeclaration key;
    VariableDeclaration lhs;
    VariableDeclaration rhs;
    Assignment cmdAssignNext;
    Assignment cmdAssignMark;

    explicit Assign(const Type& nodeType)
        : key("key", Type::data_type(), false), lhs("left", nodeType, false), rhs("right", nodeType, false),
          cmdAssignNext(std::make_unique<Dereference>(std::make_unique<VariableExpression>(lhs), "next"), std::make_unique<VariableExpression>(rhs)),
          cmdAssignMark(std::make_unique<Dereference>(std::make_unique<VariableExpression>(lhs), "mark"), std::make_unique<BooleanValue>(true)) {
    }
};

std::unique_ptr<Annotation> MakePreDelete(const SolverConfig& config, const Assign& assign) {
    auto result = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(config);
    auto mid = MakePointsTo(config);
    auto end = MakePointsTo(config);
    auto past = MakePointsTo(config);

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.key.type)));

    // heap layout: root -> mid -> end -> past
    mid->node->decl_storage = root->fieldToValue["next"]->Decl();
    end->node->decl_storage = mid->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();

    // stack
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(right->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(end->node->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(mid->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(true)
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    result->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
        ObligationAxiom::Kind::INSERT, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));

    // pushing resources
    result->now->conjuncts.push_front(std::move(past));
    result->now->conjuncts.push_front(std::move(end));
    result->now->conjuncts.push_front(std::move(mid));
    result->now->conjuncts.push_front(std::move(root));
    result->now->conjuncts.push_front(std::move(right));
    result->now->conjuncts.push_front(std::move(left));
    result->now->conjuncts.push_front(std::move(key));

    return result;
}

std::unique_ptr<Annotation> MakePreInsert(const SolverConfig& config, const Assign& assign) {
    auto result = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(config);
    auto end = MakePointsTo(config);
    auto past = MakePointsTo(config);
    auto owned = MakePointsTo(config, true);

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.key.type)));

    // heap layout: root -> end -> past, owned -> end
    end->node->decl_storage = root->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();
    owned->fieldToValue["next"]->decl_storage = end->node->Decl();

    // stack
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(right->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(owned->node->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(owned->fieldToValue["data"]->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::GT,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(owned->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    result->now->conjuncts.push_back(std::make_unique<InflowEmptinessAxiom>(owned->flow, true));

    // obligation
    result->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::INSERT, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<InflowContainsValueAxiom>(root->flow, std::make_unique<SymbolicVariable>(key->value->Decl())));

    // pushing resources
    result->now->conjuncts.push_front(std::move(past));
    result->now->conjuncts.push_front(std::move(end));
    result->now->conjuncts.push_front(std::move(root));
    result->now->conjuncts.push_front(std::move(owned));
    result->now->conjuncts.push_front(std::move(right));
    result->now->conjuncts.push_front(std::move(left));
    result->now->conjuncts.push_front(std::move(key));

    return result;
}

std::unique_ptr<Annotation> MakePreMark(const SolverConfig& config, const Assign& assign) {
    auto result = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(config);
    auto end = MakePointsTo(config);
    auto past = MakePointsTo(config);

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.rhs.type)));
    auto key = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(assign.key), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(assign.key.type)));

    // heap layout: root -> end -> past
    end->node->decl_storage = root->fieldToValue["next"]->Decl();
    past->node->decl_storage = end->fieldToValue["next"]->Decl();

    // stack
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(left->value->Decl()), SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(root->node->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(key->value->Decl()), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("data")->Decl()), SymbolicAxiom::LT,
            std::make_unique<SymbolicVariable>(past->fieldToValue.at("data")->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(root->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));
    result->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(
            std::make_unique<SymbolicVariable>(end->fieldToValue.at("mark")->Decl()), SymbolicAxiom::EQ,
            std::make_unique<SymbolicBool>(false)
    ));

    // obligation
    result->now->conjuncts.push_back(std::make_unique<ObligationAxiom>(
            ObligationAxiom::Kind::DELETE, std::make_unique<SymbolicVariable>(key->value->Decl())
    ));
    result->now->conjuncts.push_back(std::make_unique<InflowContainsValueAxiom>(root->flow, std::make_unique<SymbolicVariable>(key->value->Decl())));

    // pushing resources
    result->now->conjuncts.push_front(std::move(past));
    result->now->conjuncts.push_front(std::move(end));
    result->now->conjuncts.push_front(std::move(root));
    result->now->conjuncts.push_front(std::move(right));
    result->now->conjuncts.push_front(std::move(left));
    result->now->conjuncts.push_front(std::move(key));

    return result;
}

//
// MAIN
//

int main(int argc, char** argv) {
    auto config = MakeConfig();
    auto solver = solver::MakeDefaultSolver(config);
    Assign assign(config->flowDomain->GetNodeType());
    auto preDelete = MakePreDelete(*config, assign);
    auto preInsert = MakePreInsert(*config, assign);
    auto preMark = MakePreMark(*config, assign);

//    std::cout << "Invariant: "; heal::Print(*config->invariant->blueprint, std::cout); std::cout << std::endl << std::endl;
//    std::cout << "Flow: "; heal::Print(*config->flowDomain->GetOutFlowContains("next").blueprint, std::cout); std::cout << std::endl << std::endl;
//    std::cout << "Contains: "; heal::Print(*config->logicallyContainsKey->blueprint, std::cout); std::cout << std::endl << std::endl;

    auto start = std::chrono::steady_clock::now();
    std::size_t numRuns = 1;
    for (std::size_t count = 0; count < numRuns; ++count) {
        solver->Post(heal::Copy(*preDelete), assign.cmdAssignNext);
        solver->Post(heal::Copy(*preInsert), assign.cmdAssignNext);
        solver->Post(heal::Copy(*preMark), assign.cmdAssignMark);
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << std::endl << std::endl << "[Avg. time taken across " << numRuns << " runs: ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count()/numRuns << "ms]" << std::endl;
}
