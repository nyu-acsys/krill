#include "plankton/solver/solverimpl.hpp"

#include "cola/util.hpp"
#include "plankton/util.hpp"
#include "plankton/logger.hpp" // TODO: delete
#include "plankton/solver/post.hpp"

using namespace cola;
using namespace plankton;


std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Assume& cmd) const {
	log() << "*** PostAssume " << *cmd.expr << std::endl << *pre.now << std::endl;
	auto result = plankton::copy(pre);

	// extend result->pre and find new knowledge
	auto newAxioms = plankton::flatten(cola::copy(*cmd.expr));
	result->now = plankton::conjoin(std::move(result->now), std::move(newAxioms));
	result->now = ExtendExhaustively(std::move(result->now));
	
	// done
	log() << "~~>" << std::endl << *result->now << std::endl;
	return PostProcess(std::move(result), pre);
}
