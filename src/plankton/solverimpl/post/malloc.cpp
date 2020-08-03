#include "plankton/solverimpl/linsolver.hpp"

#include "heal/util.hpp"
#include "plankton/logger.hpp" // TODO: delete
#include "plankton/solverimpl/post/info.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;
using parallel_assignment_t = Solver::parallel_assignment_t;


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
	auto extendedPre = AddInvariant(heal::copy(pre));
	extendedPre->now = heal::conjoin(std::move(extendedPre->now), GetAllocationKnowledge(allocation));

	// establish invariant for allocation
	auto allocationInvariant = config.invariant->instantiate(allocation);
	if (!Implies(*extendedPre->now, *allocationInvariant)) {
		ThrowInvariantViolationError(cmd);
	}

	// execute a dummy assignment 'cmd.lhs = allocation'
	VariableExpression lhs(cmd.lhs), rhs(allocation);
	auto result = MakeVarAssignPost(PostInfo(*this, *extendedPre), lhs, rhs);
	assert(!heal::contains_expression(*result, rhs));

	// done
	return PostProcess(std::move(result), pre);
}

// std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Malloc& cmd) const {
// 	log() << std::endl << "ΩΩΩ POST for malloc on: " << cmd.lhs.name << std::endl;
// 	if (cmd.lhs.is_shared) ThrowUnsupportedAllocationError(cmd.lhs, "expected local variable");
	
// 	// dummy variable for allocation
// 	auto& allocation = GetDummyAllocation(cmd.lhs.type);

// 	// remove knowledge about lhs
// 	VariableExpression lhs(cmd.lhs), rhsClear(allocation);
// 	auto post = MakeVarAssignPost(PostInfo(*this, pre), lhs, rhsClear);
// 	assert(!heal::contains_expression(*post, rhsClear));

// 	// create a fresh new variable for the new allocation, extend pre with knowledge about allocation
// 	post->now = heal::conjoin(std::move(post->now), GetAllocationKnowledge(allocation));
// 	if (ImpliesFalse(*post->now)) throw std::logic_error("llllooollll1");
// 	log() << std::endl << " interim1: " << *post->now << std::endl;
// 	post->now->conjuncts.push_back(heal::MakeAxiom(heal::MakeExpr(BinaryExpression::Operator::EQ, heal::MakeExpr(cmd.lhs), heal::MakeExpr(allocation))));
// 	log() << std::endl << " interim2: " << *post->now << std::endl;
// 	if (ImpliesFalse(*post->now)) throw std::logic_error("llllooollll2");

// 	// establish invariant for allocation
// 	auto allocationInvariant = config.invariant->instantiate(allocation);
// 	if (!Implies(*post->now, *allocationInvariant)) {
// 		ThrowInvariantViolationError(cmd);
// 	}

// 	// compute post
// 	post->now = ComputeImpliedCandidates(*post->now);

// 	// done
// 	log() << std::endl << *pre.now << std::endl << " ~~>" << std::endl << *post->now << std::endl;
// 	throw std::logic_error("fjlkda");
// 	return PostProcess(std::move(post), pre);
// }
