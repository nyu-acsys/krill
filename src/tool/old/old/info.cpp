#include "prover/solverimpl/post/info.hpp"

#include <map>
#include <sstream>
#include "cola/util.hpp"
#include "heal/util.hpp"
#include "prover/error.hpp"
#include "util/logger.hpp" // TODO: delete

using namespace cola;
using namespace heal;
using namespace prover;
using parallel_assignment_t = Solver::parallel_assignment_t;


template<typename T>
inline std::string ColaToString(const T& obj) {
	std::stringstream result;
	cola::print(obj, result);
	return result.str();
}

std::string prover::AssignmentString(const Expression& lhs, const Expression& rhs) {
	return ColaToString(lhs) + " = " + ColaToString(rhs);
}

std::string prover::AssignmentString(const parallel_assignment_t& assignment) {
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

void prover::ThrowUnsupportedAssignmentError(const Expression& lhs, const Expression& rhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for assignment '" + AssignmentString(lhs, rhs) + "': " + moreReason + ".");
}

void prover::ThrowUnsupportedAllocationError(const VariableDeclaration& lhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for allocation targeting '" + lhs.name + "': " + moreReason + ".");
}


void prover::ThrowInvariantViolationError(std::string cmd, std::string more_message) {
	throw InvariantViolationError("Invariant violated" + more_message + ", at '" + cmd + "'.");
}

void prover::ThrowInvariantViolationError(const Malloc& cmd) {
	ThrowInvariantViolationError(ColaToString(cmd));
}

void prover::ThrowInvariantViolationError(const Expression& lhs, const Expression& rhs) {
	ThrowInvariantViolationError(AssignmentString(lhs, rhs));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> prover::PostProcess(std::unique_ptr<Annotation> post, const Annotation& pre) {
	// TODO: post processing required?
	// TODO: delete this or add config flag to disable it

	bool hasPreObligation = false;
	for (const auto& conjunct : pre.now->conjuncts) {
		if (dynamic_cast<const ObligationAxiom*>(conjunct.get())) {
			hasPreObligation = true;
			break;
		}
	}

	bool hasPostObligationOrFulfillment = false;
	for (const auto& conjunct : pre.now->conjuncts) {
		if (dynamic_cast<const ObligationAxiom*>(conjunct.get()) || dynamic_cast<const FulfillmentAxiom*>(conjunct.get())) {
			hasPostObligationOrFulfillment = true;
			break;
		}
	}

	if (hasPreObligation && !hasPostObligationOrFulfillment) {
		throw SolvingError("Cautiously aborting: an obligation was lost without obtaining a fulfillment.");
	}

	return post;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ConjunctionFormula> PostInfo::ComputeImpliedCandidates(const ImplicationChecker& checker) {
	if (!implicationCheck) return solver.ComputeImpliedCandidates(checker);
	assert(implication);
	return checker.ComputeImpliedConjuncts(*implication);
}

prover::PostInfo::time_container_t PostInfo::CopyPreTime() const {
	time_container_t result;
	for (const auto& predicate : preTime) {
		result.push_back(heal::copy(*predicate));
	}
	return result;
}

