#include "plankton/solver/solverimpl.hpp"

#include <map>
#include <sstream>
#include "cola/util.hpp"
#include "plankton/error.hpp"
#include "plankton/util.hpp"
#include "plankton/logger.hpp" // TODO: delete

using namespace cola;
using namespace plankton;


template<typename T>
inline std::string ColaToString(const T& obj) {
	std::stringstream result;
	cola::print(obj, result);
	return result.str();
}

inline std::string AssignmentString(const Expression& lhs, const Expression& rhs) {
	return ColaToString(lhs) + " = " + ColaToString(rhs);
}

inline void ThrowUnsupportedAssignmentError(const Expression& lhs, const Expression& rhs, std::string moreReason) {
	throw SolvingError("Cannot compute post image for assignment '" + AssignmentString(lhs, rhs) + "': " + moreReason + ".");
}

inline void ThrowUnsupportedAllocationError(const VariableDeclaration& lhs, std::string moreReason) {
	throw SolvingError("Cannot compute post image for allocation targeting '" + lhs.name + "': " + moreReason + ".");
}

inline void ThrowUnsupportedParallelAssignmentError(std::string reason) {
	throw SolvingError("Malformed parallel assignment: " + reason + ".");
}


inline void ThrowInvariantViolationError(std::string cmd, std::string more_message="") {
	throw VerificationError("Invariant violated" + more_message + ", at '" + cmd + "'.");
}

inline void ThrowInvariantViolationError(const Malloc& cmd) {
	ThrowInvariantViolationError(ColaToString(cmd));
}

inline void ThrowInvariantViolationError(const Expression& lhs, const Expression& rhs) {
	ThrowInvariantViolationError(AssignmentString(lhs, rhs));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> PostProcess(std::unique_ptr<Annotation> /*post*/, const Annotation& /*pre*/) {
	// TODO: post process post
	// TODO important: ensure that obligations are not duplicated (in post wrt. pre)
	// TODO: check if an obligation was lost while not having a fulfillment, and if so throw?
	throw std::logic_error("not yet implemented: PostProcess");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Assume& cmd) const {
	log() << "*** PostAssume " << ColaToString(*cmd.expr) << std::endl << *pre.now << std::endl;
	auto result = plankton::copy(pre);

	// extend result->pre and find new knowledge
	auto newAxioms = plankton::flatten(cola::copy(*cmd.expr));
	result->now = plankton::conjoin(std::move(result->now), std::move(newAxioms));
	result->now = ExtendExhaustively(std::move(result->now));
	
	// done
	log() << "~~>" << std::endl << *result->now << std::endl;
	return PostProcess(std::move(result), pre);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static const VariableDeclaration& GetDummyAllocation(const Type& type) {
	// keep track of dummy variables to avoid spoiling the encoder with copies
	static std::map<const Type*, std::unique_ptr<VariableDeclaration>> type2decl;
	
	// search for existing decl
	auto find = type2decl.find(&type);
	if (find != type2decl.end()) {
		return *find->second;
	}

	// create new one, remember it
	auto newDecl = std::make_unique<VariableDeclaration>("post\%alloc-ptr", type, false);
	auto result = newDecl.get();
	type2decl[&type] = std::move(newDecl);
	return *result;
}

inline std::unique_ptr<ConjunctionFormula> GetAllocationKnowledge(const VariableDeclaration& allocation) {
	auto dst = [&allocation](){ return std::make_unique<VariableExpression>(allocation); };
	auto result = std::make_unique<ConjunctionFormula>();

	// non-null
	result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::NEQ, dst(), std::make_unique<NullValue>()
	)));

	// owned
	result->conjuncts.push_back(std::make_unique<OwnershipAxiom>(dst()));

	// no flow
	result->conjuncts.push_back(std::make_unique<NegatedAxiom>(std::make_unique<HasFlowAxiom>(dst())));

	// fields are default initialized
	auto add_default = [&dst,&result](std::string fieldname, BinaryExpression::Operator op, std::unique_ptr<Expression> expr){
		result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
			op, std::make_unique<Dereference>(dst(), fieldname), std::move(expr)
		)));
	};
	for (const auto& [fieldname, type] : allocation.type.fields) {
		switch (type.get().sort) {
			case Sort::PTR:
				add_default(fieldname, BinaryExpression::Operator::EQ, std::make_unique<NullValue>());
				break;
			case Sort::BOOL:
				add_default(fieldname, BinaryExpression::Operator::EQ, std::make_unique<BooleanValue>(false));
				break;
			case Sort::DATA:
				add_default(fieldname, BinaryExpression::Operator::GT, std::make_unique<MinValue>());
				add_default(fieldname, BinaryExpression::Operator::LT, std::make_unique<MaxValue>());
				break;
			case Sort::VOID:
				break;
		}
	}

	return result;
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Malloc& cmd) const {
	if (cmd.lhs.is_shared) ThrowUnsupportedAllocationError(cmd.lhs, "expected local variable");
	
	// create a fresh new variable for the new allocation, extend pre with knowledge about allocation
	auto& allocation = GetDummyAllocation(cmd.lhs.type);
	auto extendedPre = plankton::copy(pre);
	extendedPre->now = plankton::conjoin(std::move(extendedPre->now), GetAllocationKnowledge(allocation));

	// establish invariant for allocation
	auto allocationInvariant = config.invariant->instantiate(allocation);
	if (!Implies(*extendedPre->now, *allocationInvariant)) {
		ThrowInvariantViolationError(cmd);
	}

	// execute a dummy assignment 'cmd.lhs = allocation'
	VariableExpression lhs(cmd.lhs), rhs(allocation);
	auto result = PostAssign(*extendedPre, lhs, rhs);
	assert(!plankton::contains_expression(*result, rhs));

	// done
	return PostProcess(std::move(result), pre);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Assignment& cmd) const {
	return PostProcess(PostAssign(pre, *cmd.lhs, *cmd.rhs), pre);
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, parallel_assignment_t assignments) const {
	// TODO: implemlent an optimized version that performs the assignment in one shot

	// check if we can execute the parallel assignment sequentially
	for (const auto& pair : assignments) {
		auto [isVar, lhsVar] = plankton::is_of_type<VariableExpression>(pair.first.get());

		// ensure that the left-hand side is a variable
		if (!isVar) ThrowUnsupportedParallelAssignmentError("left-hand side must be a 'VariableExpression'");

		// ensure that the left-hand side does not appear as right-hand side
		for (const auto& other : assignments) {
			if (!plankton::contains_expression(other.second.get(), *lhsVar)) continue;
			ThrowUnsupportedParallelAssignmentError("assigned variable '" + lhsVar->decl.name + "' must not appear on the right-hand side");
		}
	}

	// execute parallel assignment sequentially
	auto result = plankton::copy(pre);
	for (const auto [lhs, rhs] : assignments) {
		result = PostAssign(*result, lhs, rhs);
	}
	return PostProcess(std::move(result), pre);
}

std::unique_ptr<Annotation> SolverImpl::PostAssign(const Annotation& pre, const Expression& lhs, const Expression& rhs) const {
	auto [isLhsVar, lhsVar] = plankton::is_of_type<VariableExpression>(lhs);
	if (isLhsVar) return PostAssign(pre, *lhsVar, rhs);

	auto [isLhsDeref, lhsDeref] = plankton::is_of_type<Dereference>(lhs);
	if (!isLhsDeref) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a variable or a dereference");

	auto [isDerefVar, derefVar] = plankton::is_of_type<VariableExpression>(*lhsDeref->expr);
	if (!isDerefVar) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a dereference of a variable");

	// ensure rhs is a 'SimpleExpression' or the negation of such
	auto [isRhsNegation, rhsNegation] = plankton::is_of_type<NegatedExpression>(rhs);
	auto [isSimple, rhsSimple] = plankton::is_of_type<SimpleExpression>(isRhsNegation ? *rhsNegation->expr : rhs);
	if (!isSimple) ThrowUnsupportedAssignmentError(lhs, rhs, "expected right-hand side to be a variable or an immediate value");

	// no need to preprocess ==> done by externally accesible API
	return PostAssign(pre, *lhsDeref, rhs);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// void SolverImpl::ExtendSolverWithSpecificationCheckRules(z3::solver& /*solver*/, z3::expr_vector /*footprint*/, Sort /*sort*/) const {
// 	// TODO: distinguish between pointer and data assignments?
// 	throw std::logic_error("not yet implemented: ExtendSolverWithSpecificationCheckRules");
// }

// void SolverImpl::ExtendSolverWithStackRules(z3::solver& solver, const VariableDeclaration* changedVar) const {
// 	for (auto var : GetVariablesInScope()) {
// 		if (var == changedVar) continue;
// 		solver.add(encoder.EncodeVariable(*var) == encoder.EncodeNextVariable(*var));
// 	}
// }

// ImplicationCheckerImpl SolverImpl::MakePostChecker(const cola::VariableExpression& lhs) const {
// 	auto [solver, footprint] = encoder.MakePostSolver(0);
// 	ExtendSolverWithStackRules(solver, &lhs.decl);
// 	throw std::logic_error("not yet implemented: MakePostChecker"); // TODO: add premise/pre to solver --> via ImplicationCheckerImpl constructor
// 	return ImplicationCheckerImpl(encoder, std::move(solver), Encoder::StepTag::NEXT);
// }

// std::pair<ImplicationCheckerImpl, z3::expr_vector> SolverImpl::MakePostChecker(const cola::Dereference& lhs, std::size_t footprintSize) const {
// 	auto [solver, footprint] = encoder.MakePostSolver(footprintSize);
// 	ExtendSolverWithStackRules(solver);
// 	ExtendSolverWithSpecificationCheckRules(solver, footprint, lhs.sort());
// 	throw std::logic_error("not yet implemented: MakePostChecker"); // TODO: add premise/pre to solver --> via ImplicationCheckerImpl constructor
// 	return { ImplicationCheckerImpl(encoder, std::move(solver), Encoder::StepTag::NEXT), std::move(footprint) };
// }


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<TimePredicate>> SyntacticTimePost(const TimePredicate& /*predicate*/, const VariableExpression& /*lhs*/, const Expression& /*rhs*/) {
	throw std::logic_error("not yet implemented: SyntacticTimePost");
}

// std::unique_ptr<ConjunctionFormula> SolverImpl::ComputeAllImpliedCandidates(const ImplicationCheckerImpl& checker) const {
// 	auto result = std::make_unique<ConjunctionFormula>();
// 	for (const auto& candidate : GetCandidates().conjuncts) {
// 		if (!checker.Implies(*candidate)) continue;
// 		result->conjuncts.push_back(plankton::copy(*candidate));
// 	}
// 	return result;
// }

std::unique_ptr<Annotation> SolverImpl::PostAssign(const Annotation& /*pre*/, const VariableExpression& /*lhs*/, const Expression& /*rhs*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::PostAssign VariableExpression");

	// log() << "*** PostAssign " << AssignmentString(lhs, rhs) << std::endl << *pre.now << std::endl;
	// if (lhs.decl.is_shared) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a local variable, not shared");

	// // compute post for 'pre.now', semantically; pure stack update ==> invariant is maintained
	// auto checker = MakePostChecker(lhs);
	// throw std::logic_error("not yet implemented: PostAssign(VariableExpression) var"); // TODO: add post(lhs) = now(rhs) to checker.solver
	// auto result = std::make_unique<Annotation>(ComputeAllImpliedCandidates(checker));

	// // TODO: manually handle obligations/fulfillments?
	// throw std::logic_error("not yet implemented: PostAssign(VariableExpression) Obligation/Fulfillment");

	// // compute post for 'pre.time', syntactically
	// for (const auto& predicate : pre.time) {
	// 	auto newPredicates = SyntacticTimePost(*predicate, lhs, rhs);
	// 	result->time.insert(result->time.end(), std::make_move_iterator(newPredicates.begin()), std::make_move_iterator(newPredicates.end()));
	// }

	// log() << "~~>" << std::endl << *result->now << std::endl;
	// return result;
}

std::unique_ptr<Annotation> SolverImpl::PostAssign(const Annotation& /*pre*/, const Dereference& /*lhs*/, const Expression& /*rhs*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::PostAssign Dereference");

	// // TODO: delete lhsVar from signature
	// log() << "*** PostAssign " << AssignmentString(lhs, rhs) << std::endl << *pre.now << std::endl;

	// // create a solver
	// auto footprintSize = config.flowDomain->GetFootprintSize(pre, lhs, rhs);
	// auto [checker, footprint] = MakePostChecker(lhs, footprintSize);

	// // compute post for 'pre.now', semantically; enforce specification
	// auto result = std::make_unique<Annotation>(ComputeAllImpliedCandidates(checker));

	// // TODO: manually handle obligations/fulfillments?
	// throw std::logic_error("not yet implemented: PostAssign(Dereference) Obligation/Fulfillment");

	// // check invariant for footprint
	// for (auto node : footprint) {
	// 	if (checker.Implies(encoder.EncodeInvariant(*config.invariant, node, checker.encodingTag))) continue;
	// 	ThrowInvariantViolationError(lhs, rhs);
	// }

	// // TODO: check that footprint outflow remains unchanged
	// throw std::logic_error("not yet implemented: PostAssign(Dereference) Keyset Disjointness");

	// // copy over pre.time, it is not affected by the assignment
	// for (const auto& predicate : pre.time) {
	// 	result->time.push_back(plankton::copy(*predicate));
	// }

	// log() << "~~>" << std::endl << *result->now << std::endl;
	// return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool SolverImpl::PostEntails(const ConjunctionFormula& /*pre*/, const Assignment& /*cmd*/, const ConjunctionFormula& /*post*/) const {
	throw std::logic_error("not yet implemented: SolverImpl::PostEntails");
}







// void throw_invariant_violation_if(bool flag, const Command& command, std::string more_message="") {
// 	if (flag) {
// 		std::stringstream msg;
// 		msg << "Invariant violated" << more_message << ", at '";
// 		print(command, msg);
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

