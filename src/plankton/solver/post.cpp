#include "plankton/solver/solverimpl.hpp"

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

inline std::string AssignmentString(const Expression& lhs, const Expression& rhs) {
	return ColaToString(lhs) + " = " + ColaToString(rhs);
}

template<typename U, typename V>
inline std::string AssignmentString(const parallel_assignment_t& assignment) {
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

inline void ThrowUnsupportedAssignmentError(const Expression& lhs, const Expression& rhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for assignment '" + AssignmentString(lhs, rhs) + "': " + moreReason + ".");
}

template<typename U, typename V>
inline void ThrowUnsupportedAssignmentError(const std::vector<const U*>& lhs, const std::vector<const V*>& rhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for assignment '" + AssignmentString(lhs, rhs) + "': " + moreReason + ".");
}

inline void ThrowUnsupportedAllocationError(const VariableDeclaration& lhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for allocation targeting '" + lhs.name + "': " + moreReason + ".");
}


inline void ThrowInvariantViolationError(std::string cmd, std::string more_message="") {
	throw InvariantViolationError("Invariant violated" + more_message + ", at '" + cmd + "'.");
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

struct PostInfo {
	using time_container_t = decltype(Annotation::time);
	static const time_container_t& GetDummyContainer() { static time_container_t dummy; return dummy; }

	const SolverImpl& solver;
	const ConjunctionFormula& preNow;
	const time_container_t& preTime;
	const ConjunctionFormula& candidates;
	bool implicationCheck; // if true: do not check invariant nor specification, may abort if not all candidates are implied

	PostInfo(const SolverImpl& solver, const Annotation& pre)
		: solver(solver), preNow(*pre.now), preTime(pre.time), candidates(solver.GetCandidates()), implicationCheck(false) {}
	PostInfo(const SolverImpl& solver, const ConjunctionFormula& pre, const ConjunctionFormula& candidates)
		: solver(solver), preNow(pre), preTime(GetDummyContainer()), candidates(candidates), implicationCheck(true) {}

	std::unique_ptr<ConjunctionFormula> ComputeImpliedCandidates(const ImplicationChecker& checker);
};

struct VarPostComputer {
	PostInfo info;
	const parallel_assignment_t& assignment;

	VarPostComputer(PostInfo info_, const parallel_assignment_t& assignment) : info(std::move(info_)), assignment(assignment) {}
	std::unique_ptr<Annotation> MakePost();

	std::unique_ptr<ConjunctionFormula> MakePostNow();
	std::map<const Expression*, const VariableDeclaration*> MakeReplacementMap();
	std::unique_ptr<ConjunctionFormula> MakeInterimPostNow();

	std::unique_ptr<Annotation> MakePostTime();
};

struct DerefPostComputer {
	PostInfo info;
	const Dereference& lhs;
	const Expression& rhs;

	template<typename... Args>
	DerefPostComputer(PostInfo info_, const Dereference& lhs, const Expression& rhs) : info(std::move(info_)), lhs(lhs), rhs(rhs) {}
	std::unique_ptr<Annotation> MakePost();
};


inline std::unique_ptr<Annotation> MakePost(PostInfo info, const Expression& lhs, const Expression& rhs) {
	auto [isLhsVar, lhsVar] = plankton::is_of_type<VariableExpression>(lhs);
	if (isLhsVar) return VarPostComputer(std::move(info), { std::make_pair(std::cref(*lhsVar), std::cref(rhs)) }).MakePost();

	auto [isLhsDeref, lhsDeref] = plankton::is_of_type<Dereference>(lhs);
	if (!isLhsDeref) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a variable or a dereference");

	auto [isDerefVar, derefVar] = plankton::is_of_type<VariableExpression>(*lhsDeref->expr);
	if (!isDerefVar) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a dereference of a variable");

	// ensure rhs is a 'SimpleExpression' or the negation of such
	auto [isRhsNegation, rhsNegation] = plankton::is_of_type<NegatedExpression>(rhs);
	auto [isSimple, rhsSimple] = plankton::is_of_type<SimpleExpression>(isRhsNegation ? *rhsNegation->expr : rhs);
	if (!isSimple) ThrowUnsupportedAssignmentError(lhs, rhs, "expected right-hand side to be a variable or an immediate value");

	return DerefPostComputer(std::move(info), *lhsDeref, rhs).MakePost();
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, parallel_assignment_t assignments) const {
	// ensure that no variable is assigned multiple times
	for (std::size_t index = 0; index < assignments.size(); ++index) {
		for (std::size_t other = index+1; other < assignments.size(); ++other) {
			if (plankton::syntactically_equal(assignments[index].first, assignments[index].second)) {
				throw UnsupportedConstructError("Malformed parallel assignment: variable " + assignments[index].first.get().decl.name + " is assigned multiple times.");
			}
		}
	}

	// compute post
	return PostProcess(VarPostComputer(PostInfo(*this, pre), assignments).MakePost(), pre);
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Assignment& cmd) const {
	return PostProcess(MakePost(PostInfo(*this, pre), *cmd.lhs, *cmd.rhs), pre);
}

bool SolverImpl::PostEntails(const ConjunctionFormula& pre, const Assignment& cmd, const ConjunctionFormula& post) const {
	auto partialPostImage = MakePost(PostInfo(*this, pre, post), *cmd.lhs, *cmd.rhs);
	return partialPostImage->now->conjuncts.size() == post.conjuncts.size();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ConjunctionFormula> PostInfo::ComputeImpliedCandidates(const ImplicationChecker& checker) {
	log() << "checking: " << std::flush;
	auto result = std::make_unique<ConjunctionFormula>();
	for (const auto& candidate : candidates.conjuncts) {
		log() << "  " << *candidate << std::flush;
		if (!checker.Implies(*candidate)) {
			if (implicationCheck) break;
			continue;
		}
		result->conjuncts.push_back(plankton::copy(*candidate));
	}
	return result;
}


std::map<const Expression*, const VariableDeclaration*> VarPostComputer::MakeReplacementMap() {
	static std::map<const VariableDeclaration*, std::unique_ptr<VariableDeclaration>> decl2copy;
	static std::function<const VariableDeclaration*(const VariableDeclaration&)> GetOrCreate = [](const VariableDeclaration& decl){
		auto find = decl2copy.find(&decl);
		if (find != decl2copy.end()) return find->second.get();
		auto newDecl = std::make_unique<VariableDeclaration>("post#copy-" + decl.name, decl.type, false);
		auto result = newDecl.get();
		decl2copy[&decl] = std::move(newDecl);
		return result;
	};

	std::map<const Expression*, const VariableDeclaration*> result;
	for (const auto& pair : assignment) {
		const VariableExpression* lhsVar = &pair.first.get();
		assert(result.count(lhsVar) == 0);
		result[lhsVar] = GetOrCreate(lhsVar->decl);
	}
	return result;
}

std::unique_ptr<ConjunctionFormula> VarPostComputer::MakeInterimPostNow() {
	auto map = MakeReplacementMap();
	auto transformer = [&map](const Expression& expr) {
		std::pair<bool, std::unique_ptr<Expression>> result;
		auto find = map.find(&expr);
		result.first = find != map.end();
		if (result.first) {
			result.second = std::make_unique<VariableExpression>(*find->second);
		}
		return result;
	};
	
	// replace left-hand side variables with dummies in 'pre'
	auto result = plankton::replace_expression(plankton::copy(info.preNow), transformer);

	// add new valuation for left-hand side variables
	for (const auto& [lhs, rhs] : assignment) {
		auto rhsReplaced = plankton::replace_expression(cola::copy(rhs), transformer);
		result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::EQ, cola::copy(lhs), std::move(rhsReplaced)
		)));
	}

	// done
	return result;
}

std::unique_ptr<ConjunctionFormula> VarPostComputer::MakePostNow() {
	// interim = preNow[lhs |-> dummy] && lhs = rhs
	auto interim = MakeInterimPostNow();

	// find implied candidates
	auto checker = info.solver.MakeImplicationChecker(*interim);
	return info.ComputeImpliedCandidates(*checker);
}

std::unique_ptr<Annotation> VarPostComputer::MakePostTime() {
	throw std::logic_error("not yet implemented: VarPostComputer::MakePostTime()");
}

std::unique_ptr<Annotation> VarPostComputer::MakePost() {
	log() << "POST for: " << AssignmentString(assignment) << std::endl << info.preNow << std::endl;
	auto postNow = MakePostNow();
	auto result = MakePostTime();
	result->now = std::move(postNow);
	throw std::logic_error("breakpoint");
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> DerefPostComputer::MakePost() {
	throw std::logic_error("not yet implemented: DerefPostComputer::MakePost()");
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
	auto result = MakePost(PostInfo(*this, *extendedPre), lhs, rhs);
	assert(!plankton::contains_expression(*result, rhs));

	// done
	return PostProcess(std::move(result), pre);
}
