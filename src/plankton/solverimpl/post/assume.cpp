#include "plankton/solverimpl/linsolver.hpp"

#include "cola/util.hpp"
#include "heal/util.hpp"
#include "plankton/logger.hpp" // TODO: delete
#include "plankton/solverimpl/post/info.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;

#define BE_SYNTACTIC false


std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Assume& cmd) const {
	log() << std::endl << "ΩΩΩ POST for assume:  " << *cmd.expr << std::endl;
	auto result = heal::copy(pre);

	// extend result->pre and find new knowledge
	auto newAxioms = heal::flatten(cola::copy(*cmd.expr));
	auto combined = heal::conjoin(std::move(result->now), std::move(newAxioms));
	combined = AddInvariant(std::move(combined));
	log() << std::endl << "ΩΩΩ POST for assume:  " << *cmd.expr << std::endl << "combined pre: " << *combined << std::endl;

	// TODO: symbolic vs semantic
	// #if BE_SYNTACTIC
		// result->now = std::move(combined);
	// #else
		auto checker = MakeImplicationChecker(*combined);
		result->now = ComputeImpliedCandidates(*checker); // TODO: execute a dummy assignment x=x instead, to reuse assignment logic?
	// #endif
	
	// done
	log() << *pre.now << std::endl << " ~~> " << std::endl << *result->now << std::endl;
	return PostProcess(std::move(result), pre);
}
