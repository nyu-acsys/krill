#include <memory>
#include <iostream>
#include <fstream>
#include <chrono>
#include "tclap/CmdLine.h"

#include "cola/parse.hpp"
#include "cola/ast.hpp"
#include "cola/util.hpp"
#include "cola/transform.hpp"

// #include "types/preprocess.hpp"
// #include "types/rmraces.hpp"
// #include "types/check.hpp"
// #include "types/cave.hpp"

using namespace TCLAP;
using namespace cola;
// using namespace prtypes;


//
// HELPERS
//
using timepoint_t = std::chrono::steady_clock::time_point;
using duration_t = std::chrono::milliseconds;

static const duration_t ZERO_DURATION = std::chrono::milliseconds(0);

timepoint_t get_time() {
	return std::chrono::steady_clock::now();
}

duration_t get_elapsed(timepoint_t since) {
	timepoint_t now = get_time();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now - since);
}

std::string to_ms(duration_t duration) {
	return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) + "ms";
}

std::string to_s(duration_t duration) {
	auto count = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	auto seconds = count / 1000;
	auto milli = count - seconds;
	while (milli >= 100) {
		milli = milli / 10;
	}
	return std::to_string(seconds) + "." + std::to_string(milli) + "s";
}

struct IsRegularFileConstraint : public Constraint<std::string> {
	std::string id = "path";
	std::string description() const override { return "path to regular file"; }
 	std::string shortID() const override { return this->id; }
 	bool check(const std::string& path) const override {
 		std::ifstream stream(path.c_str());
    	return stream.good();
 	}
 	IsRegularFileConstraint(std::string more_verbose="") {
 		this->id += more_verbose;
 	}
};

static void fail_if(bool condition, CmdLine& cmd, std::string description="undefined exception", std::string id="undefined") {
	if (condition) {
		SpecificationException error(description, id);
		cmd.getOutput()->failure(cmd, error);
	}
}


//
// CONFIG / OUTPUT
//
struct PlanktonConfig {
	std::shared_ptr<Program> program;
	std::string program_path;
	bool quiet, verbose;
	bool print_gist;
} config;

struct PlanktonOutput {
	// verification results, time, ...
	bool verification_success = false;
	duration_t time_verification = ZERO_DURATION;
} output;


//
// HANDLING INPUT
//
static void read_input() {
	// parse program
	try {
		std::cout << std::endl << "Parsing file... " << std::flush;
		config.program = cola::parse_program(config.program_path);
		std::cout << "done" << std::endl;

	} catch (ParseError err) {
		std::cout << "failed" << std::endl;
		std::cout << std::endl << std::endl;
		std::cout << "## Could not parse file ==> aborting" << std::endl;
		std::cout << "## Reason:" << std::endl;
		std::cout << err.what() << std::endl;
		return;
	}

	// handle program options
	Program& program = *config.program;
	// TODO: extract/handle program options
	
	// preprocess program
	try {
		std::cout << "Preprocessing program... " << std::flush;
		cola::simplify(program);
		std::cout << "done" << std::endl;
		program.name += " (preprocessed)";

	} catch (TransformationError err) {
		std::cout << "failed" << std::endl;
		std::cout << std::endl << std::endl;
		std::cout << "## Could not preprocess program ==> aborting" << std::endl;
		std::cout << "## Reason:" << std::endl;
		std::cout << err.what() << std::endl;
		return;
	}
}

static void print_program() {
	// print program
	std::cout << std::endl << std::endl;
	cola::print(*config.program, std::cout);
	std::cout << std::endl << std::endl;
}


//
// VERIFICATION
//
static void verify() {
	throw std::logic_error("not yet implement (verify)");
}


//
// OUTPUT
//
static void print_summary() {
	if (config.quiet) { return; }
	std::cout << std::endl << std::endl;
	std::cout << "# Summary:" << std::endl;
	std::cout << "# ========" << std::endl;
	std::cout << "# Verification Result:             " << (output.verification_success ? "SUCCESS" : "FAILED") << std::endl;
	std::cout << "# Verification Time:               " << to_s(output.time_verification) << std::endl;
}

static void print_gist() {
	if (!config.print_gist) { return; }
	throw std::logic_error("not yet implement (print_gist");
}


//
// MAIN
//
int main(int argc, char** argv) {
	try {
		CmdLine cmd("SEAL verification tool for lock-free data structures with safe memory reclamation", ' ', "0.1α");
		auto is_program_constraint = std::make_unique<IsRegularFileConstraint>("_to_program");

		SwitchArg quiet_switch("q", "quiet", "Disables most output", cmd, false);
		SwitchArg verbose_switch("v", "verbose", "Verbose output", cmd, false);
		SwitchArg gist_switch("g", "gist", "Print machine readable gist at the very end", cmd, false);
		UnlabeledValueArg<std::string> program_arg("program", "Input program file to analyze", true, "", is_program_constraint.get(), cmd);

		cmd.parse(argc, argv);
		config.program_path = program_arg.getValue();
		config.quiet = quiet_switch.getValue();
		config.verbose = verbose_switch.getValue();
		config.print_gist = gist_switch.getValue();

		// sanity checks
		fail_if(config.quiet && config.verbose, cmd, "Quiet and verbose mode cannot be used together.");

	} catch (ArgException &e) {
		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
		return -1;
	}

	if (config.verbose) { throw std::logic_error("Verbose mode not yet implemented"); }
	if (config.quiet) { throw std::logic_error("Quiet mode not yet implemented"); }

	read_input();
	if (!config.program) { return -1; }
	print_program();

	verify();
	print_summary();
	print_gist();
	return 0;
}
