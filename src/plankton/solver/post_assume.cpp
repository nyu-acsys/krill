#include "plankton/solver/solverimpl.hpp"

#include "cola/util.hpp"
#include "plankton/util.hpp"
#include "plankton/logger.hpp" // TODO: delete
#include "plankton/solver/post.hpp"

using namespace cola;
using namespace plankton;


std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Assume& cmd) const {
	log() << std::endl << "ΩΩΩ POST for assume:  " << *cmd.expr << std::endl;
	auto result = plankton::copy(pre);

	// extend result->pre and find new knowledge
	auto newAxioms = plankton::flatten(cola::copy(*cmd.expr));
	auto combined = plankton::conjoin(std::move(result->now), std::move(newAxioms));
	auto checker = MakeImplicationChecker(*combined);
	result->now = ComputeImpliedCandidates(*checker); // TODO: execute a dummy assignment x=x instead, to reuse assignment logic?
	
	// done
	// log() << *pre.now << std::endl << " ~~> " << std::endl << *result->now << std::endl;
	return PostProcess(std::move(result), pre);
}
