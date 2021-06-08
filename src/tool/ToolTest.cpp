#include <chrono>
#include <iomanip>
#include "cola/ast.hpp"
#include "cola/util.hpp"
#include "cola/parse.hpp"
#include "cola/transform.hpp"
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

const std::string NEXT_FIELD = "next";
const std::string DATA_FIELD = "val";

SymbolicFactory factory;


//
// SOLVER
//

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
        auto isNotTail = std::make_unique<SymbolicAxiom>( // assume node is global
                std::make_unique<SymbolicVariable>(node.fieldToValue.at(DATA_FIELD)->Decl()),
                SymbolicAxiom::LT, std::make_unique<SymbolicMax>()
        );
        auto nextNotNull = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at(NEXT_FIELD)->Decl()),
                SymbolicAxiom::NEQ, std::make_unique<SymbolicNull>()
        );
        auto dataInInflow = std::make_unique<InflowContainsValueAxiom>(
                node.flow, heal::Copy(*node.fieldToValue.at(DATA_FIELD))
        );
        blueprint->conjuncts.push_back(std::make_unique<SeparatingImplication>(
                heal::Copy(*nextNotNull), std::move(dataInInflow)
        ));
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
//        auto unmarked = std::make_unique<SymbolicAxiom>(
//                std::make_unique<SymbolicVariable>(node.fieldToValue.at("mark")->Decl()),
//                SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(false)
//        );
        auto data = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at(DATA_FIELD)->Decl()),
                SymbolicAxiom::EQ, std::make_unique<SymbolicVariable>(vars[0])
        );
        auto blueprint = std::make_unique<SeparatingConjunction>();
//        blueprint->conjuncts.push_back(std::move(unmarked));
        blueprint->conjuncts.push_back(std::move(data));
        return blueprint;
    });
}

std::unique_ptr<Predicate> MakeOutflowPredicate(const Type& nodeType, const Type& flowType) {
    return MakePredicate(nodeType, flowType, "outflow", [](auto& node, auto& vars){
//        auto marked = std::make_unique<SymbolicAxiom>(
//                std::make_unique<SymbolicVariable>(node.fieldToValue.at("mark")->Decl()),
//                SymbolicAxiom::EQ, std::make_unique<SymbolicBool>(true)
//        );
        auto unmarked = std::make_unique<SymbolicAxiom>(
                std::make_unique<SymbolicVariable>(node.fieldToValue.at(DATA_FIELD)->Decl()),
                SymbolicAxiom::LT, std::make_unique<SymbolicVariable>(vars[0])
        );
//        auto disjunction = std::make_unique<StackDisjunction>();
//        disjunction->axioms.push_back(std::move(marked));
//        disjunction->axioms.push_back(std::move(unmarked));
        auto blueprint = std::make_unique<SeparatingConjunction>();
//        blueprint->conjuncts.push_back(std::move(disjunction));
        blueprint->conjuncts.push_back(std::move(unmarked));
        return blueprint;
    });
}

struct TestFlowDomain : public FlowDomain {
    const Type& nodeType;
    std::unique_ptr<Predicate> outflow;

    explicit TestFlowDomain(const Type& nodeType) : nodeType(nodeType), outflow(MakeOutflowPredicate(nodeType, Type::data_type())) {}

    [[nodiscard]] const cola::Type& GetNodeType() const override { return nodeType; }
    [[nodiscard]] const cola::Type& GetFlowValueType() const override { return Type::data_type(); }
    [[nodiscard]] const Predicate& GetOutFlowContains(std::string fieldName) const override { return *outflow; }
};

std::shared_ptr<SolverConfig> MakeConfig(const Program& program) {
    assert(program.types.size() == 1);
    auto& nodeType = *program.types.at(0);
    auto domain = std::make_unique<TestFlowDomain>(nodeType);
    auto contains = MakeContainsPredicate(domain->GetNodeType(), domain->GetFlowValueType());
    auto invariant = MakeInvariant(domain->GetNodeType(), domain->GetFlowValueType());
    return std::make_shared<SolverConfig>(FOOTPRINT_DEPTH, std::move(domain), std::move(contains), std::move(invariant), INVARIANT_FOR_LOCAL_RESOURCES);
}

//
// MAIN
//

std::shared_ptr<Program> ReadInput(const std::string& filename, bool preprocess=true) {
    std::shared_ptr<Program> result;

    // parse program
    try {
        std::cout << std::endl << "Parsing file... " << std::flush;
        result = cola::parse_program(filename);
        std::cout << "done" << std::endl;

    } catch (ParseError& err) {
        std::cout << "failed" << std::endl;
        std::cout << std::endl << std::endl;
        std::cout << "## Could not parse file ==> aborting" << std::endl;
        std::cout << "## Reason:" << std::endl;
        std::cout << err.what() << std::endl;
        throw err;
    }

    // preprocess program
    if (preprocess) {
        try {
            std::cout << "Preprocessing program... " << std::flush;
            cola::simplify(*result);
            std::cout << "done" << std::endl;
            result->name += " (preprocessed)";

        } catch (TransformationError& err) {
            std::cout << "failed" << std::endl;
            std::cout << std::endl << std::endl;
            std::cout << "## Could not preprocess program ==> aborting" << std::endl;
            std::cout << "## Reason:" << std::endl;
            std::cout << err.what() << std::endl;
            throw err;
        }
    }

    assert(result);
    return result;
}

int main(int argc, char** argv) {
//    bool preprocess = false;
//    std::string file = "/Users/wolff/Desktop/plankton/examples/transformed.cola";
    bool preprocess = true;
    std::string file = "/Users/wolff/Desktop/plankton/examples/VechevDCasSet.cola";

    auto program = ReadInput(file, preprocess);
    auto config = MakeConfig(*program);
//    cola::print(*program, std::cout);

    auto start = std::chrono::steady_clock::now();
    auto isLinearizable = prover::CheckLinearizability(*program, config);
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    std::cout << "Test " << file << ": is " << (isLinearizable ? "" : "NOT") << " linearizable" << std::endl;
    std::cout << "Time taken: " << elapsed << "ms" << std::endl;
}
