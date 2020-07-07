#include "plankton/solver/solverimpl.hpp"

#include <iostream> // TODO: delete

using namespace cola;
using namespace plankton;


std::unique_ptr<Annotation> SolverImpl::Post(const Formula& /*pre*/, const cola::Assume& /*cmd*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::Post Assume");
}

std::unique_ptr<Annotation> SolverImpl::Post(const Formula& /*pre*/, const cola::Malloc& /*cmd*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::Post Malloc");
}

std::unique_ptr<Annotation> SolverImpl::Post(const Formula& /*pre*/, const cola::Assignment& /*cmd*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::Post Assignment");
}

std::unique_ptr<Annotation> SolverImpl::Post(const Formula& /*pre*/, parallel_assignment_t /*assignment*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::Post parallel_assignment_t");
}


bool SolverImpl::PostEntails(const Formula& /*pre*/, const cola::Assume& /*cmd*/, const Formula& /*post*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::PostEntails");
}

bool SolverImpl::PostEntails(const Formula& /*pre*/, const cola::Malloc& /*cmd*/, const Formula& /*post*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::PostEntails");
}

bool SolverImpl::PostEntails(const Formula& /*pre*/, const cola::Assignment& /*cmd*/, const Formula& /*post*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::PostEntails");
}
