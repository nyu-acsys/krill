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


SymbolicFactory factory;

std::unique_ptr<PointsToAxiom> MakePointsTo(const Type& nodeType, const Type& flowType) {
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fields;
    for (const auto& [name, type] : nodeType.fields) {
        fields.emplace(name, std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type)));
    }
    auto node = std::make_unique<PointsToAxiom>(
            std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(nodeType)), false,
            factory.GetUnusedFlowVariable(flowType), std::move(fields)
    );
    return node;
}

std::unique_ptr<PointsToAxiom> MakePointsTo(const SolverConfig& config) {
    return MakePointsTo(config.flowDomain->GetNodeType(), config.flowDomain->GetFlowValueType());
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
        auto blueprint = std::make_unique<FlatSeparatingConjunction>();
        // TODO: insert dummy invariant
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
        auto blueprint = std::make_unique<FlatSeparatingConjunction>();
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
        auto blueprint = std::make_unique<FlatSeparatingConjunction>();
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
    return std::make_shared<SolverConfig>(1, std::move(domain), std::move(contains), std::move(invariant));
}

//
// PRE
//

struct Assign {
    VariableDeclaration lhs;
    VariableDeclaration rhs;
    Assignment cmd;

    explicit Assign(const Type& nodeType) : lhs("left", nodeType, false), rhs("right", nodeType, false),
        cmd(std::make_unique<Dereference>(std::make_unique<VariableExpression>(lhs), "next"), std::make_unique<VariableExpression>(rhs)) {
    }
};

std::unique_ptr<Annotation> MakePre(const SolverConfig& config, const VariableDeclaration& lhs, const VariableDeclaration& rhs) {
    auto result = std::make_unique<Annotation>();

    // heap resources
    auto root = MakePointsTo(config);
    auto mid = MakePointsTo(config);
    auto end = MakePointsTo(config);
    auto past = MakePointsTo(config);

    // variable resources
    auto left = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(lhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(lhs.type)));
    auto right = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(rhs), std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(rhs.type)));

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

    result->now->conjuncts.push_front(std::move(past));
    result->now->conjuncts.push_front(std::move(end));
    result->now->conjuncts.push_front(std::move(mid));
    result->now->conjuncts.push_front(std::move(root));
    result->now->conjuncts.push_front(std::move(right));
    result->now->conjuncts.push_front(std::move(left));

    return result;
}

//
// MAIN
//

int main(int argc, char** argv) {
    auto config = MakeConfig();
    auto solver = solver::MakeDefaultSolver(config);
    Assign assign(config->flowDomain->GetNodeType());
    auto pre = MakePre(*config, assign.lhs, assign.rhs);

//    std::cout << "Invariant: "; heal::Print(*config->invariant->blueprint, std::cout); std::cout << std::endl << std::endl;
//    std::cout << "Flow: "; heal::Print(*config->flowDomain->GetOutFlowContains("next").blueprint, std::cout); std::cout << std::endl << std::endl;
//    std::cout << "Contains: "; heal::Print(*config->logicallyContainsKey->blueprint, std::cout); std::cout << std::endl << std::endl;
//    std::cout << "Assign: "; cola::print(assign.cmd, std::cout); std::cout << std::endl;
//    std::cout << "Pre: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl << std::endl;
//    std::cout << std::flush;

    auto [post, effect] = solver->Post(std::move(pre), assign.cmd);
}
