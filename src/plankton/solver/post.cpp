#include "plankton/solver/post.hpp"

#include <map>
#include <sstream>
#include "cola/util.hpp"
#include "plankton/error.hpp"
#include "plankton/util.hpp"
#include "plankton/logger.hpp" // TODO: delete

using namespace cola;
using namespace plankton;
using parallel_assignment_t = Solver::parallel_assignment_t;


template<typename T>
inline std::string ColaToString(const T& obj) {
	std::stringstream result;
	cola::print(obj, result);
	return result.str();
}

std::string plankton::AssignmentString(const Expression& lhs, const Expression& rhs) {
	return ColaToString(lhs) + " = " + ColaToString(rhs);
}

std::string plankton::AssignmentString(const parallel_assignment_t& assignment) {
	std::string result = "[";
	bool first = true;
	for (auto [lhs, rhs] : assignment) {
		if (!first) result += "; ";
		first = false;
		result += AssignmentString(lhs, rhs);
	}
	result += "]";
	return result;
}

void plankton::ThrowUnsupportedAssignmentError(const Expression& lhs, const Expression& rhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for assignment '" + AssignmentString(lhs, rhs) + "': " + moreReason + ".");
}

void plankton::ThrowUnsupportedAllocationError(const VariableDeclaration& lhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for allocation targeting '" + lhs.name + "': " + moreReason + ".");
}


void plankton::ThrowInvariantViolationError(std::string cmd, std::string more_message) {
	throw InvariantViolationError("Invariant violated" + more_message + ", at '" + cmd + "'.");
}

void plankton::ThrowInvariantViolationError(const Malloc& cmd) {
	ThrowInvariantViolationError(ColaToString(cmd));
}

void plankton::ThrowInvariantViolationError(const Expression& lhs, const Expression& rhs) {
	ThrowInvariantViolationError(AssignmentString(lhs, rhs));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> plankton::PostProcess(std::unique_ptr<Annotation> /*post*/, const Annotation& /*pre*/) {
	// TODO: post process post
	// TODO important: ensure that obligations are not duplicated (in post wrt. pre)
	// TODO: check if an obligation was lost while not having a fulfillment, and if so throw?
	throw std::logic_error("not yet implemented: PostProcess");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ConjunctionFormula> PostInfo::ComputeImpliedCandidates(const ImplicationChecker& checker) {
	auto result = std::make_unique<ConjunctionFormula>();
	for (const auto& candidate : candidates.conjuncts) {
		if (!checker.Implies(*candidate)) {
			if (implicationCheck) break;
			continue;
		}
		result->conjuncts.push_back(plankton::copy(*candidate));
	}
	log() << "postNow: " << *result << std::endl;
	return result;
}


