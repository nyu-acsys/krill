#include "plankton/solver/solverimpl.hpp"

#include <map>
#include <sstream>
#include <iostream> // TODO: delete
#include "cola/util.hpp"
#include "plankton/error.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


std::unique_ptr<Annotation> PostProcess(std::unique_ptr<Annotation> /*post*/, const Annotation& /*pre*/) {
	// TODO: post process post
	// TODO important: ensure that obligations are not duplicated (in post wrt. pre)
	throw std::logic_error("not yet implemented: PostProcess");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const cola::Assume& cmd) const {
	auto result = plankton::copy(pre);

	// extend result->pre and find new knowledge
	auto newAxioms = plankton::flatten(cola::copy(*cmd.expr));
	result->now = plankton::conjoin(std::move(result->now), std::move(newAxioms));
	result->now = ExtendExhaustively(std::move(result->now));
	
	// done
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

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const cola::Malloc& cmd) const {
	if (cmd.lhs.is_shared) {
		throw SolvingError("Cannot compute post for allocation: expected local variable as target, got shared variable '" + cmd.lhs.name + "'.");
	}
	
	// create a fresh new variable for the new allocation, extend pre with knowledge about allocation
	auto& allocation = GetDummyAllocation(cmd.lhs.type);
	auto extendedPre = plankton::copy(pre);
	extendedPre->now = plankton::conjoin(std::move(extendedPre->now), GetAllocationKnowledge(allocation));

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

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const cola::Assignment& cmd) const {
	return PostProcess(PostAssign(pre, *cmd.lhs, *cmd.rhs), pre);
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, parallel_assignment_t assignments) const {
	// check if we can execute the parallel assignment sequentially
	for (const auto& pair : assignments) {
		auto [isVar, lhsVar] = plankton::is_of_type<VariableExpression>(pair.first.get());

		// ensure that the left-hand side is a variable
		if (!isVar) {
			throw SolvingError("Malformed parallel assignment: left-hand side must be a 'VariableExpression'.");
		}

		// ensure that the left-hand side does not appear as right-hand side
		for (const auto& other : assignments) {
			if (plankton::contains_expression(other.second.get(), *lhsVar)) {
				throw SolvingError("Malformed parallel assignment: assigned variable '" + lhsVar->decl.name + "' must not appear on the right-hand side.");
			}
		}
	}

	// execute parallel assignment sequentially
	// TODO: implemlent an optimized version that performs the assignment in one shot
	auto result = plankton::copy(pre);
	for (const auto [lhs, rhs] : assignments) {
		result = PostAssign(*result, lhs, rhs);
	}
	return PostProcess(std::move(result), pre);
}

inline std::string AssignmentString(const Expression& lhs, const Expression& rhs) {
	std::stringstream result;
	cola::print(lhs, result);
	result << " = ";
	cola::print(rhs, result);
	return result.str();
}

inline void ThrowUnsupportedAssignmentError(const Expression& lhs, const Expression& rhs, std::string moreReason) {
	throw SolvingError("Cannot compute post image for assignment '" + AssignmentString(lhs, rhs) + "': " + moreReason + ".");
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
	return PostAssign(pre, *lhsDeref, *derefVar, rhs);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void SolverImpl::ExtendSolverWithSpecificationCheckRules(z3::solver& /*solver*/, z3::expr_vector /*footprint*/) const {
	// TODO: distinguish between pointer and data assignments?
	throw std::logic_error("not yet implemented: ExtendSolverWithSpecificationCheckRules");
}

void SolverImpl::ExtendSolverWithStackRules(z3::solver& solver, const VariableDeclaration* changedVar) const {
	for (auto var : GetVariablesInScope()) {
		if (var == changedVar) continue;
		solver.add(encoder.EncodeVariable(*var) == encoder.EncodeNextVariable(*var));
	}
}

std::unique_ptr<Annotation> SolverImpl::PostAssign(const Annotation& pre, const cola::VariableExpression& lhs, const cola::Expression& rhs) const {
	if (lhs.decl.is_shared) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a local variable, not shared");
	auto result = std::make_unique<Annotation>();

	std::cout << "*** PostAssign " << AssignmentString(lhs, rhs) << std::endl;
	plankton::print(*pre.now, std::cout);
	std::cout << std::endl;

	auto [solver, footprint] = encoder.MakePostSolver(0); // only stack is changed ==> empty footprint
	ExtendSolverWithStackRules(solver, &lhs.decl);
	// TODO: rules for obligations/fulfillments missing
	ImplicationCheckerImpl checker(encoder, std::move(solver), Encoder::StepTag::NEXT);

	// compute 'result->now'
	for (const auto& candidate : GetCandidates().conjuncts) {
		if (!checker.Implies(*candidate)) continue;
		result->now->conjuncts.push_back(plankton::copy(*candidate));
	}

	// compute 'result->time'
	// TODO: implement ==> syntactically inline equality and remove evertying containing syntactically old value?

	throw std::logic_error("not yet implemented: PostAssignVar");
}

std::unique_ptr<Annotation> SolverImpl::PostAssign(const Annotation& /*pre*/, const cola::Dereference& /*lhs*/, const cola::VariableExpression& /*lhsVar*/, const cola::Expression& /*rhs*/) const {
	throw std::logic_error("not yet implemented: PostAssignDeref");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

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

