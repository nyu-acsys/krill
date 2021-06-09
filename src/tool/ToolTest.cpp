#include <chrono>
#include <iomanip>
#include "cola/ast.hpp"
#include "cola/util.hpp"
#include "cola/parse.hpp"
#include "cola/transform.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"
#include "solver/config.hpp"
#include "solver/solver.hpp"

#include "prover/verify.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


constexpr std::size_t FOOTPRINT_DEPTH = 1;

const std::string NEXT_FIELD = "next";
const std::string DATA_FIELD = "val";
const std::string HEAD_NAME = "Head";

SymbolicFactory factory;


//
// SOLVER
//

template<SymbolicAxiom::Operator OP, typename I, typename... Args>
inline std::unique_ptr<SymbolicAxiom> MakeImmediateAxiom(const SymbolicVariableDeclaration& decl, Args&&... args) {
    return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(decl), OP, std::make_unique<I>(std::forward<Args>(args)...));
}

inline std::unique_ptr<Formula> MakeSharedInvariant(const PointsToAxiom& memory, const VariableDeclaration& head) {
    auto result = std::make_unique<SeparatingConjunction>();
//    auto isHead = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicMin>(memory.fieldToValue.at(DATA_FIELD)->Decl());
    auto isHead = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(head), std::make_unique<SymbolicVariable>(memory.node->Decl()));
//    auto isTail = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicMax>(memory.fieldToValue.at(DATA_FIELD)->Decl());
//    auto nextNull = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicNull>(memory.fieldToValue.at(NEXT_FIELD)->Decl());
    auto nextNotNull = MakeImmediateAxiom<SymbolicAxiom::NEQ, SymbolicNull>(memory.fieldToValue.at(NEXT_FIELD)->Decl());
    auto dataInInflow = std::make_unique<InflowContainsValueAxiom>(memory.flow, heal::Copy(*memory.fieldToValue.at(DATA_FIELD)));
    result->conjuncts.push_back(std::make_unique<SeparatingImplication>(std::move(isHead), heal::Copy(*nextNotNull)));
//    result->conjuncts.push_back(std::make_unique<SeparatingImplication>(std::move(isTail), std::move(nextNull)));
    result->conjuncts.push_back(std::make_unique<SeparatingImplication>(std::move(nextNotNull), std::move(dataInInflow)));
    return result;
}

inline std::unique_ptr<Formula> MakeLocalInvariant(const PointsToAxiom& memory) {
    return std::make_unique<InflowEmptinessAxiom>(memory.flow, true);
}

inline std::unique_ptr<Formula> MakeNodeInvariant(const PointsToAxiom& memory, const VariableDeclaration& head) {
    if (memory.isLocal) return MakeLocalInvariant(memory);
    else return MakeSharedInvariant(memory, head);
}

inline std::unique_ptr<Formula> MakeVariableInvariant(const EqualsToAxiom& variable) {
    auto result = std::make_unique<SeparatingConjunction>();
    if (!variable.variable->decl.is_shared) return result;
    result->conjuncts.push_back(MakeImmediateAxiom<SymbolicAxiom::NEQ, SymbolicNull>(variable.value->Decl()));
//    auto isHead = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(head), std::make_unique<SymbolicVariable>(variable.value->Decl()));
    return result;
}

inline std::unique_ptr<Formula> MakeOutflow(const PointsToAxiom& memory, const std::string& fieldName, const SymbolicVariableDeclaration& value) {
    assert(fieldName == NEXT_FIELD);
    return MakeImmediateAxiom<SymbolicAxiom::LT, SymbolicVariable>(memory.fieldToValue.at(DATA_FIELD)->Decl(), value);
}

inline std::unique_ptr<Formula> MakeContains(const PointsToAxiom& memory, const SymbolicVariableDeclaration& value) {
    return MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicVariable>(memory.fieldToValue.at(DATA_FIELD)->Decl(), value);
}

struct TestSolverConfig : public SolverConfig {
    const Type& nodeType;
    const VariableDeclaration& head;
    explicit TestSolverConfig(const Type& nodeType, const VariableDeclaration& head) : nodeType(nodeType), head(head) {}

    [[nodiscard]] inline const cola::Type& GetFlowValueType() const override { return Type::data_type(); }
    [[nodiscard]] inline std::size_t GetMaxFootprintDepth(const std::string& /*updatedFieldName*/) const override { return FOOTPRINT_DEPTH; }
    [[nodiscard]] inline std::unique_ptr<Formula> GetNodeInvariant(const PointsToAxiom& memory) const override { return MakeNodeInvariant(memory, head); }
    [[nodiscard]] inline std::unique_ptr<Formula> GetSharedVariableInvariant(const EqualsToAxiom& variable) const override { return MakeVariableInvariant(variable); }
    [[nodiscard]] inline std::unique_ptr<Formula> GetOutflowContains(const PointsToAxiom& memory, const std::string& fieldName, const SymbolicVariableDeclaration& value) const override { return MakeOutflow(memory, fieldName, value); }
    [[nodiscard]] inline std::unique_ptr<Formula> GetLogicallyContains(const PointsToAxiom& memory, const SymbolicVariableDeclaration& value) const override { return MakeContains(memory, value); }
};

std::shared_ptr<SolverConfig> MakeConfig(const Program& program) {
    assert(program.types.size() == 1);
    auto& nodeType = *program.types.at(0);
    assert(program.variables.size() == 2);
    assert(program.variables.at(0)->name == HEAD_NAME);
    auto& head = *program.variables.at(0);
    return std::make_shared<TestSolverConfig>(nodeType, head);
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

int main(int /*argc*/, char** /*argv*/) {
//    bool preprocess = false;
//    std::string file = "/Users/wolff/Desktop/plankton/examples/transformed.cola";
    bool preprocess = true;
    std::string file = "/Users/wolff/Desktop/plankton/examples/VechevDCasSet.cola";

    auto program = ReadInput(file, preprocess);
    auto config = MakeConfig(*program);
//    cola::print(*program, std::cout);
//    throw std::logic_error("breakpoint");

    auto start = std::chrono::steady_clock::now();
    auto isLinearizable = prover::CheckLinearizability(*program, config);
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    std::cout << "Test " << file << ": is " << (isLinearizable ? "" : "NOT") << " linearizable" << std::endl;
    std::cout << "Time taken: " << elapsed << "ms" << std::endl;
}
