#include <chrono>
#include <utility>
#include "tclap/CmdLine.h"
#include "cfg2string.hpp"
#include "engine/linearizability.hpp"
#include "engine/setup.hpp"
#include "parser/parse.hpp"
#include "util/log.hpp"


using namespace plankton;


//
// Command Line
//

struct CommandLineInput {
    std::string pathToInput;
    std::optional<std::string> pathToOutput;
    std::size_t repetitions;
};

struct IsRegularFileConstraint : public TCLAP::Constraint<std::string> {
    std::string id = "path";
    [[nodiscard]] std::string description() const override { return "path to regular file"; }
    [[nodiscard]] std::string shortID() const override { return this->id; }
    [[nodiscard]] bool check(const std::string& path) const override {
        std::ifstream stream(path.c_str());
        return stream.good();
    }
    explicit IsRegularFileConstraint(const std::string& more_verbose="") {
        this->id += more_verbose;
    }
};

inline CommandLineInput Interact(int argc, char** argv) {
    CommandLineInput input;

    TCLAP::CmdLine cmd("PLANKTON verification tool for lock-free data structures", ' ', "1.0");
    auto isFile = std::make_unique<IsRegularFileConstraint>("_to_input");

    TCLAP::UnlabeledValueArg<std::string> inputArg("input", "Input file", true, "", isFile.get(), cmd);
    TCLAP::ValueArg<std::string> outputArg("o", "output", "Output file", false, "", isFile.get(), cmd);
    TCLAP::ValueArg<std::size_t> repArg("r", "repetitions", "Number of repetitions", false, 100, "integer", cmd);

    cmd.parse(argc, argv);
    input.pathToInput = inputArg.getValue();
    input.repetitions = repArg.getValue();
    if (outputArg.isSet()) input.pathToOutput = outputArg.getValue();
    else input.pathToOutput = std::nullopt;

    return input;
}


//
// Parsing
//

inline FlowConstraintsParsingResult Parse(const CommandLineInput& input) {
    return plankton::ParseFlowConstraints(input.pathToInput);
}


//
// Evaluation
//

inline std::string SizeOf(const std::optional<NodeSet>& footprint) {
    if (!footprint.has_value()) return "-1";
    return std::to_string(footprint->size());
}

inline void Evaluate(std::ostream& output, const FlowConstraint& graph, std::size_t rep, std::string testName, const std::string& methodName,
                     const std::function<Evaluation(const FlowConstraint&)>& method) {
    auto eval = method(graph);
    if (testName.empty()) testName = "--";
    output << methodName << "," << SizeOf(eval.footprint) << "," << eval.time.count() << ",\"" << testName << "\"," << rep << ",\"" << graph.name << "\"" << std::endl;
}

inline void Evaluate(const FlowConstraintsParsingResult& input, const CommandLineInput& config) {
    std::ofstream fileOutput;
    if (config.pathToOutput.has_value()) fileOutput.open(config.pathToOutput.value(), std::ios::app);
    bool toFile = fileOutput.is_open();
    auto& output = toFile ? fileOutput : std::cout;

    if (toFile) {
        INFO("Read " << input.constraints.size() << " flow constraints." << std::endl)
        INFO("Performing " << config.repetitions << " repetitions." << std::endl)
        INFO("Running evaluation... " << std::flush)
    }
    std::size_t counter = 0;
    std::size_t total = input.constraints.size() * config.repetitions;
    auto percentage = [&]() -> std::size_t { return ((counter * 1.0) / (total * 1.0)) * 100.0; };
    for (std::size_t rep = 0; rep < config.repetitions; ++rep) {
        for (const auto& graph: input.constraints) {
            Evaluate(output, *graph, rep, input.name, "General", Evaluate_GeneralMethod_NoAcyclicityCheck);
            Evaluate(output, *graph, rep, input.name, "General+Acyclicity", Evaluate_GeneralMethod_WithAcyclicityCheck);
            Evaluate(output, *graph, rep, input.name, "New", Evaluate_NewMethod_AllPaths);
            Evaluate(output, *graph, rep, input.name, "New+Dec", Evaluate_NewMethod_DiffPaths);
            Evaluate(output, *graph, rep, input.name, "New+Dec+Idem", Evaluate_NewMethod_DiffPathsIndividually);
            if (counter++ % 50 == 0 && toFile) INFO(percentage() << "% " << std::flush)
        }
    }
    if (toFile) INFO("100% done!" << std::endl)
}


//
// Main
//

int main(int argc, char** argv) {
    try {
        auto cmd = Interact(argc, argv);
        auto input = Parse(cmd);
        Evaluate(input, cmd);
        return 0;

    } catch (TCLAP::ArgException& err) {
        // command line misuse
        INFO("ERROR: " << err.error() << " for arg " << err.argId() << std::endl << std::endl)
        ERROR(err.error() << " for arg " << err.argId() << std::endl)
        return 1;

    } catch (std::logic_error& err) { // TODO: catch proper error class
        // plankton error
        INFO(std::endl << std::endl << "ERROR: " << err.what() << std::endl << std::endl)
        ERROR(err.what() << std::endl)
        return 2;
    }
}
