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
	throw UnsupportedConstructError("Cannot compute post image for assignment '" + AssignmentString(lhs, rhs) + "': " + moreReason + ".");
}

inline void ThrowUnsupportedAllocationError(const VariableDeclaration& lhs, std::string moreReason) {
	throw UnsupportedConstructError("Cannot compute post image for allocation targeting '" + lhs.name + "': " + moreReason + ".");
}

inline void ThrowUnsupportedParallelAssignmentError(std::string reason) {
	throw UnsupportedConstructError("Malformed parallel assignment: " + reason + ".");
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

template<typename T>
class AssignmentPostComputer {
	private:
		const PostConfig& config;
		Encoder& encoder;
		const ConjunctionFormula& candidates;
		const std::deque<const cola::VariableDeclaration*>& variablesInScope;
		const Annotation& pre;
		const T& lhs;
		const cola::Expression& rhs;
		bool justEntailment;

	private:
		std::size_t GetFootprintSize();
		void ExtendSolverWithStackRules(z3::solver& solver);
		void ExtendSolverWithHeapRules(z3::solver& solver, z3::expr_vector footprint);
		std::pair<ImplicationCheckerImpl, z3::expr_vector> MakePostChecker();
		std::unique_ptr<ConjunctionFormula> ComputeAllImpliedCandidates(const ImplicationCheckerImpl& checker);
		std::unique_ptr<ConjunctionFormula> ComputePostSpecification(const ImplicationCheckerImpl& checker);
		void CheckInvariant(const ImplicationCheckerImpl& checker, z3::expr_vector footprint);
		std::unique_ptr<ConjunctionFormula> MakePostNow();
		std::unique_ptr<Annotation> MakePostTime(std::unique_ptr<ConjunctionFormula> now);

	public:
		AssignmentPostComputer(const PostConfig& config, Encoder& encoder, const ConjunctionFormula& candidates,
			const std::deque<const cola::VariableDeclaration*>& variablesInScope, const Annotation& pre,
			const T& lhs, const cola::Expression& rhs, bool justEntailment=false)
		: config(config), encoder(encoder), candidates(candidates), variablesInScope(variablesInScope), pre(pre),
		  lhs(lhs), rhs(rhs), justEntailment(justEntailment)
		{}

		std::unique_ptr<Annotation> MakePost();
};

template<>
std::size_t AssignmentPostComputer<VariableExpression>::GetFootprintSize() {
	return 0;
}

template<>
std::size_t AssignmentPostComputer<Dereference>::GetFootprintSize() {
	auto size = config.flowDomain->GetFootprintSize(pre, lhs, rhs);
	if (size == 0) throw SolvingError("Heap updates cannot have an empty footprint.");
	return size;
}

template<typename T>
void AssignmentPostComputer<T>::ExtendSolverWithHeapRules(z3::solver& /*solver*/, z3::expr_vector /*footprint*/) {
	if (std::is_same_v<T, VariableExpression>) return;

	// TODO: add lhs.expr->{selector} changes/non-changes
	// TODO: invoke invariant for footprint?
	throw std::logic_error("not yet implemented: ExtendSolverWithSpecificationCheckRules");
}

template<>
void AssignmentPostComputer<VariableExpression>::ExtendSolverWithStackRules(z3::solver& solver) {
	solver.add(encoder.EncodeNext(lhs) == encoder.Encode(rhs));
	for (auto var : variablesInScope) {
		if (var == &lhs.decl) continue;
		solver.add(encoder.EncodeVariable(*var) == encoder.EncodeNextVariable(*var));
	}
}

template<>
void AssignmentPostComputer<Dereference>::ExtendSolverWithStackRules(z3::solver& solver) {
	for (auto var : variablesInScope) {
		solver.add(encoder.EncodeVariable(*var) == encoder.EncodeNextVariable(*var));
	}
}

template<typename T>
std::pair<ImplicationCheckerImpl, z3::expr_vector> AssignmentPostComputer<T>::MakePostChecker() {
	auto footprintSize = GetFootprintSize();
	auto [solver, footprint] = encoder.MakePostSolver(footprintSize);
	ExtendSolverWithStackRules(solver);
	ExtendSolverWithHeapRules(solver, footprint);
	return { ImplicationCheckerImpl(encoder, *pre.now, std::move(solver), Encoder::StepTag::NEXT), std::move(footprint) };
}

inline bool SkipForEntailmentCheck(const SimpleFormula& formula) {
	return plankton::is_of_type<SpecificationAxiom>(formula).first;
}

template<typename T>
std::unique_ptr<ConjunctionFormula> AssignmentPostComputer<T>::ComputeAllImpliedCandidates(const ImplicationCheckerImpl& checker) {
	auto result = std::make_unique<ConjunctionFormula>();
	for (const auto& candidate : candidates.conjuncts) {
		if (!checker.Implies(*candidate)) {
			if (!justEntailment) continue;
			if (SkipForEntailmentCheck(*candidate)) continue;
			break;
		}
		result->conjuncts.push_back(plankton::copy(*candidate));
	}
	return result;
}

template<typename T>
std::unique_ptr<ConjunctionFormula> AssignmentPostComputer<T>::ComputePostSpecification(const ImplicationCheckerImpl& /*checker*/) {
	// TODO: manually handle obligations/fulfillments?
	throw std::logic_error("not yet implemented: AssignmentPostComputer<T>::ComputePostSpecification");
}

template<>
void AssignmentPostComputer<VariableExpression>::CheckInvariant(const ImplicationCheckerImpl& /*checker*/, z3::expr_vector /*footprint*/) {
	if (lhs.decl.is_shared) {
		ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a local variable, not shared");
	}
}

template<>
void AssignmentPostComputer<Dereference>::CheckInvariant(const ImplicationCheckerImpl& checker, z3::expr_vector footprint) {
	for (auto node : footprint) {
		if (checker.Implies(encoder.EncodeInvariant(*config.invariant, node, checker.encodingTag))) continue;
		ThrowInvariantViolationError(lhs, rhs);
	}
}

template<typename T>
std::unique_ptr<ConjunctionFormula> AssignmentPostComputer<T>::MakePostNow() {
	auto [checker, footprint] = MakePostChecker();
	if (!justEntailment) CheckInvariant(checker, footprint);

	auto post = ComputeAllImpliedCandidates(checker);
	if (justEntailment) return post;

	auto spec = ComputePostSpecification(checker);
	return plankton::conjoin(std::move(post), std::move(spec));
}

template<>
std::unique_ptr<Annotation> AssignmentPostComputer<VariableExpression>::MakePostTime(std::unique_ptr<ConjunctionFormula> now) {
	auto result = std::make_unique<Annotation>(std::move(now));
	throw std::logic_error("not yet implemented: AssignmentPostComputer<>::MakePostTime");
	// TODO: fastpath for pre.time.empty()
	// std::vector<std::unique_ptr<TimePredicate>> SyntacticTimePost(const TimePredicate& /*predicate*/, const VariableExpression& /*lhs*/, const Expression& /*rhs*/)
	// for (const auto& predicate : pre.time) {
	// 	auto newPredicates = SyntacticTimePost(*predicate, lhs, rhs);
	// 	result->time.insert(result->time.end(), std::make_move_iterator(newPredicates.begin()), std::make_move_iterator(newPredicates.end()));
	// }
	return result;
}

template<>
std::unique_ptr<Annotation> AssignmentPostComputer<Dereference>::MakePostTime(std::unique_ptr<ConjunctionFormula> now) {
	auto result = std::make_unique<Annotation>(std::move(now));
	for (const auto& predicate : pre.time) {
		result->time.push_back(plankton::copy(*predicate));
	}
	return result;
}


template<typename T>
std::unique_ptr<Annotation> AssignmentPostComputer<T>::MakePost() {
	log() << "*** PostAssign " << AssignmentString(lhs, rhs) << std::endl << *pre.now << std::endl;
	auto result = MakePostTime(MakePostNow());
	log() << "~~>" << std::endl << *result->now << std::endl;
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> MakeAssignmentPost(const PostConfig& config, Encoder& encoder, const ConjunctionFormula& candidates,
	const std::deque<const cola::VariableDeclaration*>& variablesInScope, const Annotation& pre, const Expression& lhs,
	const cola::Expression& rhs, bool justEntailment=false)
{
	// auto makePost = [&](const auto& expr)

	auto [isLhsVar, lhsVar] = plankton::is_of_type<VariableExpression>(lhs);
	if (isLhsVar) {
		return AssignmentPostComputer<VariableExpression>(config, encoder, candidates, variablesInScope, pre, *lhsVar, rhs, justEntailment).MakePost();
	}

	auto [isLhsDeref, lhsDeref] = plankton::is_of_type<Dereference>(lhs);
	if (!isLhsDeref) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a variable or a dereference");

	auto [isDerefVar, derefVar] = plankton::is_of_type<VariableExpression>(*lhsDeref->expr);
	if (!isDerefVar) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a dereference of a variable");

	// ensure rhs is a 'SimpleExpression' or the negation of such
	auto [isRhsNegation, rhsNegation] = plankton::is_of_type<NegatedExpression>(rhs);
	auto [isSimple, rhsSimple] = plankton::is_of_type<SimpleExpression>(isRhsNegation ? *rhsNegation->expr : rhs);
	if (!isSimple) ThrowUnsupportedAssignmentError(lhs, rhs, "expected right-hand side to be a variable or an immediate value");

	return AssignmentPostComputer<Dereference>(config, encoder, candidates, variablesInScope, pre, *lhsDeref, rhs, justEntailment).MakePost();
}

std::unique_ptr<Annotation> SolverImpl::PostAssign(const Annotation& pre, const cola::Expression& lhs, const cola::Expression& rhs) const {
	return MakeAssignmentPost(config, encoder, GetCandidates(), GetVariablesInScope(), pre, lhs, rhs);
}

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

bool SolverImpl::PostEntails(const ConjunctionFormula& pre, const Assignment& cmd, const ConjunctionFormula& post) const {
	Annotation dummyPre(plankton::copy(pre)); // TODO: avoid the copy --> have AssignmentPostComputer take now and time separately? better take an annotation as argument
	auto partialPostImage = MakeAssignmentPost(config, encoder, GetCandidates(), GetVariablesInScope(), dummyPre, *cmd.lhs, *cmd.rhs, true);
	return partialPostImage->now->conjuncts.size() == post.conjuncts.size(); // TODO: is this correct?
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
