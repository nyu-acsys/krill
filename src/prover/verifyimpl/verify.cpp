#include "../../../include/prover/verify.hpp"


using namespace cola;
using namespace heal;
using namespace prover;


bool prover::check_linearizability(const Program& program) {
	Verifier verifier;
	program.accept(verifier);
	return true;
}


Effect::Effect(std::unique_ptr<heal::ConjunctionFormula> pre_, std::unique_ptr<cola::Assignment> cmd_)
	: precondition(std::move(pre_)), command(std::move(cmd_)) {}