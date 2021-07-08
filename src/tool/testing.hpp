#pragma once
#ifndef TESTING
#define TESTING

#include <chrono>
#include "cola/ast.hpp"
#include "cola/util.hpp"
#include "cola/parse.hpp"
#include "cola/transform.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"
#include "solver/config.hpp"
#include "solver/solver.hpp"
#include "prover/verify.hpp"


struct Benchmark {
    static constexpr std::size_t FOOTPRINT_DEPTH = 1;

    std::shared_ptr<cola::Program> program;
    const cola::Type& nodeType;
    const cola::VariableDeclaration* head;
    const cola::VariableDeclaration* tail;

    struct BenchmarkConfig : public solver::SolverConfig {
        const Benchmark& benchmark;
        explicit BenchmarkConfig(const Benchmark& benchmark) : benchmark(benchmark) {}
        [[nodiscard]] inline const cola::Type& GetFlowValueType() const override { return cola::Type::data_type(); }
        [[nodiscard]] inline std::size_t GetMaxFootprintDepth(const std::string& /*updatedFieldName*/) const override { return FOOTPRINT_DEPTH; }
        [[nodiscard]] inline std::unique_ptr<heal::Formula> GetNodeInvariant(const heal::PointsToAxiom& memory) const override { return benchmark.MakeNodeInvariant(memory); }
        [[nodiscard]] inline std::unique_ptr<heal::Formula> GetSharedVariableInvariant(const heal::EqualsToAxiom& variable) const override { return benchmark.MakeVariableInvariant(variable); }
        [[nodiscard]] inline std::unique_ptr<heal::Formula> GetOutflowContains(const heal::PointsToAxiom& memory, const std::string& fieldName, const heal::SymbolicVariableDeclaration& value) const override { return benchmark.MakeOutflow(memory, fieldName, value); }
        [[nodiscard]] inline std::unique_ptr<heal::Formula> GetLogicallyContains(const heal::PointsToAxiom& memory, const heal::SymbolicVariableDeclaration& value) const override { return benchmark.MakeContains(memory, value); }
    };
    std::shared_ptr<BenchmarkConfig> config;

    explicit Benchmark(const std::string& code, const std::string& nodeTypeName,
                       std::optional<std::string> headName, std::optional<std::string> tailName)
            : program(ParseCode(code)), nodeType(Find(program->types, nodeTypeName + "*")),
              head(headName.has_value() ? &Find(program->variables, headName.value()) : nullptr),
              tail(tailName.has_value() ? &Find(program->variables, tailName.value()) : nullptr),
              config(std::make_shared<BenchmarkConfig>(*this)) {}

    inline int operator()() const {
        cola::print(*program, std::cout);
        // throw std::logic_error("breakpoint");

        auto start = std::chrono::steady_clock::now();
        auto isLinearizable = prover::CheckLinearizability(*program, config);
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        std::cout << std::endl << std::endl;
        std::cout << "!@#$%^&*()" << std::endl;
        std::cout << std::endl << std::endl;
        std::cout << "##########" << std::endl;
        std::cout << "## Test " << program->name << ": is " << (isLinearizable ? "" : "NOT") << " linearizable" << std::endl;
        std::cout << "## Time taken: " << elapsed << "ms" << std::endl;
        std::cout << "##########" << std::endl << std::endl;

        return isLinearizable ? 0 : -1;
    }

    inline void operator()(std::size_t repetitions) const {
        std::deque<std::string> reports;
        auto start = std::chrono::steady_clock::now();
        for (std::size_t count = 0; count < repetitions; ++count) {
            try {
                prover::CheckLinearizability(*program, config);
                reports.emplace_back("SUCCESS");
            } catch (std::logic_error& err) {
                reports.emplace_back(err.what());
            }
        }
        auto end = std::chrono::steady_clock::now();
        std::cout << std::endl << std::endl;
        std::cout << "##########" << std::endl;
        std::cout << std::endl << std::endl << "## Reports:" << std::endl;
        for (const auto& elem : reports) std::cout << "##   -> " << elem << std::endl;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end-start).count();
        std::cout << "## Time taken: " << elapsed << "s  (avg " << elapsed/repetitions << "s)" << std::endl;
        std::cout << "##########" << std::endl << std::endl;
    }

    [[nodiscard]] virtual std::unique_ptr<heal::Formula> MakeSharedNodeInvariant(const heal::PointsToAxiom& memory, const cola::VariableDeclaration* head, const cola::VariableDeclaration* tail) const = 0;
    [[nodiscard]] virtual std::unique_ptr<heal::Formula> MakeLocalNodeInvariant(const heal::PointsToAxiom& memory) const = 0;
    [[nodiscard]] virtual std::unique_ptr<heal::Formula> MakeVariableInvariant(const heal::EqualsToAxiom& variable) const = 0;
    [[nodiscard]] virtual std::unique_ptr<heal::Formula> MakeOutflow(const heal::PointsToAxiom& memory, const std::string& fieldName, const heal::SymbolicVariableDeclaration& value) const = 0;
    [[nodiscard]] virtual std::unique_ptr<heal::Formula> MakeContains(const heal::PointsToAxiom& memory, const heal::SymbolicVariableDeclaration& value) const = 0;

private:
    [[nodiscard]] static inline std::shared_ptr<cola::Program> ParseCode(const std::string& implementation) {
        try {
            std::cout << std::endl << "Parsing program... " << std::flush;
            std::istringstream stream(implementation);
            auto program = cola::parse_program(stream);
            std::cout << "done" << std::endl;
            std::cout << "Preprocessing program... " << std::flush;
            cola::simplify(*program);
            std::cout << "done" << std::endl;
            program->name += " (preprocessed)";
            return program;
        } catch (cola::ParseError& err) {
            std::cout << "failed" << std::endl;
            std::cout << std::endl << std::endl;
            std::cout << "## Could not parse file ==> aborting" << std::endl;
            std::cout << "## Reason:" << std::endl;
            std::cout << err.what() << std::endl;
            throw std::move(err);
        } catch (cola::TransformationError& err) {
            std::cout << "failed" << std::endl;
            std::cout << std::endl << std::endl;
            std::cout << "## Could not preprocess program ==> aborting" << std::endl;
            std::cout << "## Reason:" << std::endl;
            std::cout << err.what() << std::endl;
            throw std::move(err);
        }
    }

    template<typename T>
    [[nodiscard]] static inline const T& Find(const std::vector<std::unique_ptr<T>>& container, const std::string& name) {
        for (const auto& elem : container) {
            if (elem->name == name) return *elem;
        }
        throw std::logic_error("Cannot find '" + name + "'.");
    }

    [[nodiscard]] inline std::unique_ptr<heal::Formula> MakeNodeInvariant(const heal::PointsToAxiom& memory) const {
        if (memory.isLocal) return MakeLocalNodeInvariant(memory);
        else return MakeSharedNodeInvariant(memory, head, tail);
    }

};

#endif
