#include "plankton/solver/solverimpl.hpp"

#include <iostream> // TODO: delete
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


std::unique_ptr<Annotation> PostProcess(std::unique_ptr<Annotation> /*post*/, const Annotation& /*pre*/) {
	// TODO: post process post
	// TODO important: ensure that obligations are not duplicated (in post wrt. pre)
	throw std::logic_error("not yet implemented: PostProcess");
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const cola::Assume& cmd) const {
	auto result = plankton::copy(pre);

	// extend result->pre
	auto newAxioms = plankton::flatten(cola::copy(*cmd.expr));
	result->now = plankton::conjoin(std::move(result->now), std::move(newAxioms));

	// find new knowledge
	auto checker = MakeImplicationChecker(*result->now);
	auto newConjuncts = std::make_unique<ConjunctionFormula>();
	for (const auto& conjunct : GetCandidates().conjuncts) {
		if (plankton::syntactically_contains_conjunct(*result->now, *conjunct)) continue;
		if (!checker->Implies(*conjunct)) continue;
		newConjuncts->conjuncts.push_back(plankton::copy(*conjunct));
	}
	result->now = plankton::conjoin(std::move(result->now), std::move(newConjuncts));
	
	// done
	return PostProcess(std::move(result), pre);
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& /*pre*/, const cola::Malloc& /*cmd*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::Post Malloc");
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& /*pre*/, const cola::Assignment& /*cmd*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::Post Assignment");
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, parallel_assignment_t /*assignment*/) const {
	std::cout << "*** Post parallel_assignment_t" << std::endl;
	plankton::print(pre, std::cout);
	std::cout << std::endl;

	auto e = encoder.Encode(*pre.now);
	std::cout << e << std::endl;

	throw std::logic_error("not yet implemented: SolverImpl::Post parallel_assignment_t");
}

bool SolverImpl::PostEntails(const ConjunctionFormula& /*pre*/, const cola::Assignment& /*cmd*/, const ConjunctionFormula& /*post*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::PostEntails");
}







// void throw_invariant_violation_if(bool flag, const Command& command, std::string more_message="") {
// 	if (flag) {
// 		std::stringstream msg;
// 		msg << "Invariant violated" << more_message << ", at '";
// 		cola::print(command, msg);
// 		msg << "'.";
// 		throw VerificationError(msg.str());
// 	}
// }

// void Verifier::check_invariant_stability(const Assignment& command) {
// 	throw_invariant_violation_if(!post_maintains_invariant(*current_annotation, command), command);
// }

// void Verifier::check_invariant_stability(const Malloc& command) {
// 	if (command.lhs.is_shared) {
// 		throw UnsupportedConstructError("allocation targeting shared variable '" + command.lhs.name + "'");
// 	}
// 	throw_invariant_violation_if(!plankton::post_maintains_invariant(*current_annotation, command), command, " by newly allocated node");
// }

