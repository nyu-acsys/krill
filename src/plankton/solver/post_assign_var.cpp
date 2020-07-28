#include "plankton/solver/post.hpp"

#include <map>
#include "plankton/logger.hpp" // TODO: delete
#include "cola/util.hpp"
#include "plankton/util.hpp"
#include "plankton/solver/solverimpl.hpp"

using namespace cola;
using namespace plankton;
using parallel_assignment_t = Solver::parallel_assignment_t;


struct VarPostComputer {
	PostInfo info;
	const parallel_assignment_t& assignment;

	VarPostComputer(PostInfo info_, const parallel_assignment_t& assignment) : info(std::move(info_)), assignment(assignment) {}
	std::unique_ptr<Annotation> MakePost();

	// bool AddSolvingRules();
	// transformer_t MakeNowTransformer();
	// std::unique_ptr<ConjunctionFormula> MakeInterimPostNow();
	std::unique_ptr<ConjunctionFormula> MakePostNow();

	std::unique_ptr<PastPredicate> MakeInterimPostTime(const PastPredicate& predicate);
	std::unique_ptr<FuturePredicate> MakeInterimPostTime(const FuturePredicate& predicate);
	std::unique_ptr<Annotation> MakePostTime();
	transformer_t MakeTimeTransformer();
};

std::unique_ptr<Annotation> VarPostComputer::MakePost() {
	log() << std::endl << "ΩΩΩ POST for assignment: " << AssignmentString(assignment) << std::endl;
	if (info.solver.ImpliesFalseQuick(info.preNow)) {
		log() << "    => pruning: premise is false" << std::endl;
		return Annotation::make_false();
	}
	if (assignment.empty()) {
		log() << "    => pruning: empty assignment" << std::endl;
		return std::make_unique<Annotation>(plankton::copy(info.preNow), info.CopyPreTime());
	}

	auto postNow = MakePostNow();
	auto result = MakePostTime();
	result->now = std::move(postNow);
	// log() << info.preNow << std::endl << " ~~>" << std::endl << *result->now << std::endl;
	return result;
}

std::unique_ptr<Annotation> plankton::MakeVarAssignPost(PostInfo info, const Solver::parallel_assignment_t& assignment) {
	return VarPostComputer(std::move(info), assignment).MakePost();
}

std::unique_ptr<Annotation> plankton::MakeVarAssignPost(PostInfo info, const VariableExpression& lhs, const Expression& rhs) {
	return MakeVarAssignPost(std::move(info), { std::make_pair(std::cref(lhs), std::cref(rhs)) });
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// bool VarPostComputer::AddSolvingRules() { // TODO: what? what does it do??
// 	for (auto [lhs, rhs] : assignment) {
// 		if (plankton::is_of_type<VariableExpression>(rhs.get()).first) {
// 			return true;
// 		}
// 	}
// 	return false;
// }

// transformer_t VarPostComputer::MakeNowTransformer() {
// 	// lookup table to avoid spoinling the encoder
// 	static std::map<const VariableDeclaration*, std::unique_ptr<VariableDeclaration>> decl2copy;
// 	static std::function<const VariableDeclaration*(const VariableDeclaration&)> GetOrCreate = [](const VariableDeclaration& decl){
// 		auto find = decl2copy.find(&decl);
// 		if (find != decl2copy.end()) return find->second.get();
// 		auto newDecl = std::make_unique<VariableDeclaration>("post#copy-" + decl.name, decl.type, false);
// 		auto result = newDecl.get();
// 		decl2copy[&decl] = std::move(newDecl);
// 		return result;
// 	};

// 	// create map
// 	std::map<const VariableDeclaration*, const VariableDeclaration*> map;
// 	for (const auto& pair : assignment) {
// 		const VariableDeclaration* lhsVar = &pair.first.get().decl;
// 		assert(map.count(lhsVar) == 0);
// 		map[lhsVar] = GetOrCreate(*lhsVar);
// 	}
	
// 	// create transformer
// 	return [map = std::move(map)](const Expression& expr) {
// 		std::pair<bool, std::unique_ptr<Expression>> result;
// 		result.first = false;

// 		auto [isVar, var] = plankton::is_of_type<VariableExpression>(expr);
// 		if (isVar) {
// 			auto find = map.find(&var->decl);
// 			if (find != map.end()) {
// 				result.first = true;
// 				result.second = std::make_unique<VariableExpression>(*find->second);
// 			}
// 		}

// 		return result;
// 	};
// }

// std::unique_ptr<ConjunctionFormula> VarPostComputer::MakeInterimPostNow() {
// 	auto transformer = MakeNowTransformer();
	
// 	// replace left-hand side variables with dummies in 'pre'
// 	auto result = info.solver.AddInvariant(plankton::copy(info.preNow));
// 	if (AddSolvingRules()) result = info.solver.AddRules(std::move(result));
// 	result = plankton::replace_expression(std::move(result), transformer);

// 	// add new valuation for left-hand side variables
// 	for (const auto& [lhs, rhs] : assignment) {
// 		auto rhsReplaced = plankton::replace_expression(cola::copy(rhs), transformer);
// 		result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
// 			BinaryExpression::Operator::EQ, cola::copy(lhs), std::move(rhsReplaced)
// 		)));
// 	}

// 	// done
// 	return result;
// }

// std::unique_ptr<ConjunctionFormula> VarPostComputer::MakePostNow() {
// 	// interim = preNow[lhs |-> dummy] && lhs = rhs
// 	auto interim = MakeInterimPostNow();

// 	// find implied candidates
// 	auto checker = info.solver.MakeImplicationChecker(*interim);
// 	auto post = info.ComputeImpliedCandidates(*checker);
// 	// TODO important: remove ownership / add knowledge that certain things are not owned

// 	// done
// 	return post;
// }

static std::string SortToString(Sort sort) { // TODO: code duplication
	switch (sort) {
		case Sort::VOID: return "VOID";
		case Sort::BOOL: return "BOOL";
		case Sort::DATA: return "DATA";
		case Sort::PTR: return "PTR";
	}
}

template<Sort S, std::size_t I = 0>
static const VariableDeclaration& GetDummyDecl() {  // TODO: code duplication
	static Type dummyType("dummy-post-deref-" + SortToString(S) + "-type", Sort::PTR);
	static VariableDeclaration dummyDecl("post-deref#dummy-" + SortToString(S) + "-var" + std::to_string(I), dummyType, false);
	return dummyDecl;
}

std::unique_ptr<ConjunctionFormula> VarPostComputer::MakePostNow() {
	// TODO: combine both post mechanisms
	//    1. split pre into (ownership/lhs containing formula, rest)
	//    2. use old method for rest
	//    3. use new method for ... containing formula
	//    4. combine
	//    also note: ownership can only be lost

	static ExpressionAxiom trueAxiom(std::make_unique<BooleanValue>(true));
	auto& encoder = info.solver.GetEncoder();
	auto solver = encoder.MakeSolver();
	ImplicationCheckerImpl checker(encoder, solver, trueAxiom, Encoder::StepTag::NEXT);

	// variables
	auto qvNode = encoder.EncodeVariable(GetDummyDecl<Sort::PTR>());
	auto qvKey = encoder.EncodeVariable(GetDummyDecl<Sort::DATA>());

	// add current state
	solver.add(encoder.Encode(info.preNow));
	solver.add(encoder.Encode(info.solver.GetInstantiatedRules()));
	solver.add(encoder.Encode(info.solver.GetInstantiatedInvariant()));

	// heap remains unchanged
	solver.add(z3::forall(qvNode, encoder.EncodeTransitionMaintainsHeap(qvNode, info.solver.config.flowDomain->GetNodeType())));

	// flow remains unchanged
	solver.add(z3::forall(qvNode, qvKey, encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)));

	// ownership // TODO: is this stuff correct?
	auto vec = encoder.MakeVector();
	for (auto [lhs, rhs] : assignment) {
		if (rhs.get().sort() != Sort::PTR) continue;
		auto [isRhsVar, rhsVar] = plankton::is_of_type<VariableExpression>(rhs.get());
		if (isRhsVar && !rhsVar->decl.is_shared && !lhs.get().decl.is_shared) continue;
		auto encodedRhs = encoder.Encode(rhs);
		vec.push_back(qvNode != encodedRhs);
		solver.add(encoder.EncodeNextOwnership(encodedRhs, false));
	}
	solver.add(z3::forall(qvNode, encoder.MakeImplication(
		encoder.MakeAnd(vec), /* ==> */ encoder.EncodeTransitionMaintainsOwnership(qvNode)
	)));

	// stack
	for (auto var : info.solver.GetVariablesInScope()) {
		bool isAssigned = false;
		for (auto [lhs, rhs] : assignment) {
			if (&lhs.get().decl != var) continue;
			isAssigned = true;
			break;
		}
		if (isAssigned) continue;

		solver.add(encoder.EncodeVariable(*var) == encoder.EncodeNextVariable(*var));
	}

	for (auto [lhs, rhs] : assignment) {
		solver.add(encoder.EncodeNextVariable(lhs.get().decl) == encoder.Encode(rhs.get()));
	}

	// done
	return info.ComputeImpliedCandidates(checker);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

transformer_t VarPostComputer::MakeTimeTransformer() {
	// create map
	std::map<const VariableDeclaration*, const Expression*> map;
	for (const auto& pair : assignment) {
		const VariableDeclaration* lhs = &pair.first.get().decl;
		const Expression* rhs = &pair.second.get();
		map[lhs] = rhs;
	}
	
	// create transformer
	return [map = std::move(map)](const Expression& expr) {
		std::pair<bool, std::unique_ptr<Expression>> result;
		result.first = false;

		auto [isVar, var] = plankton::is_of_type<VariableExpression>(expr);
		if (isVar) {
			auto find = map.find(&var->decl);
			if (find != map.end()) {
				result.first = true;
				result.second = cola::copy(*find->second);
			}
		}

		return result;
	};
}

std::unique_ptr<PastPredicate> VarPostComputer::MakeInterimPostTime(const PastPredicate& predicate) {
	auto result = plankton::copy(predicate);

	// remove conjuncts containing lhs
	result->formula = plankton::remove_conjuncts_if(std::move(result->formula), [this](const SimpleFormula& conjunct){
		for (auto [lhs, rhs] : assignment) {
			if (plankton::contains_expression(conjunct, lhs)) {
				return true;
			}
		}
		return false;
	});

	return result;
}

std::unique_ptr<FuturePredicate> VarPostComputer::MakeInterimPostTime(const FuturePredicate& predicate) {
	return plankton::copy(predicate);
}

struct TimeChooser : public BaseLogicVisitor {
	const PastPredicate* past = nullptr;
	const FuturePredicate* future = nullptr;
	void visit(const PastPredicate& formula) override { past = &formula; future = nullptr; }
	void visit(const FuturePredicate& formula) override { future = &formula; past = nullptr; }
	std::tuple<bool, const PastPredicate*, const FuturePredicate*> Choose(const TimePredicate& predicate) {
		predicate.accept(*this);
		assert(!past ^ !future);
		return { past != nullptr, past, future };
	}
};

std::unique_ptr<Annotation> VarPostComputer::MakePostTime() {
	auto result = std::make_unique<Annotation>();
	auto transformer = MakeTimeTransformer();
	TimeChooser chooser;

	for (const auto& predicate : info.preTime) {
		auto [isPast, past, future] = chooser.Choose(*predicate);

		// make post image
		std::unique_ptr<TimePredicate> post;
		if (isPast) post = MakeInterimPostTime(*past);
		else post = MakeInterimPostTime(*future);

		// copy post image, replace rhs with lhs
		// TODO: explore all combinations of replacements for parallel assignments?
		bool wasChanged = true;
		auto copy = plankton::replace_expression(plankton::copy(*post), transformer/*, &wasChanged*/);

		// add post and copy
		result->time.push_back(std::move(post));
		if (wasChanged) result->time.push_back(std::move(copy));
	}

	return result;
}
