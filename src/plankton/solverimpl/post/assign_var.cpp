#include "plankton/solverimpl/linsolver.hpp"

#include <map>
#include "cola/util.hpp"
#include "heal/util.hpp"
#include "plankton/logger.hpp" // TODO: delete
#include "plankton/util.hpp"
#include "plankton/solverimpl/post/info.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;
using parallel_assignment_t = Solver::parallel_assignment_t;


struct VarPostComputer {
	PostInfo info;
	const parallel_assignment_t& assignment;

	VarPostComputer(PostInfo info_, const parallel_assignment_t& assignment) : info(std::move(info_)), assignment(assignment) {}

	std::unique_ptr<Annotation> MakePost();
	std::unique_ptr<ConjunctionFormula> MakePostNow();
	std::unique_ptr<PastPredicate> MakeInterimPostTime(const PastPredicate& predicate);
	std::unique_ptr<FuturePredicate> MakeInterimPostTime(const FuturePredicate& predicate);
	std::unique_ptr<Annotation> MakePostTime();
	transformer_t MakeTimeTransformer();
};

std::unique_ptr<Annotation> VarPostComputer::MakePost() {
	log() << std::endl << "ΩΩΩ POST for assignment: " << AssignmentString(assignment) << std::endl;
	if (assignment.empty()) {
		log() << "    => pruning: empty assignment" << std::endl;
		return std::make_unique<Annotation>(heal::copy(info.preNow), info.CopyPreTime());
	}
	if (info.solver.ImpliesFalseQuick(info.preNow)) {
		log() << "    => pruning: premise is false" << std::endl;
		return Annotation::make_false();
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

std::unique_ptr<ConjunctionFormula> VarPostComputer::MakePostNow() {
	auto checkerOwner = info.solver.MakeImplicationChecker(EncodingTag::NEXT);
	auto encoderPtr = info.solver.GetEncoder();
	auto& nodeType = info.solver.config.flowDomain->GetNodeType();
	auto& encoder = *encoderPtr;
	auto& checker = *checkerOwner;

	// quantified variables
	auto [qvNode, qvKey] = plankton::MakeQuantifiedVariables(encoderPtr, nodeType, Type::data_type());

	// add current state
	checker.AddPremise(encoder.EncodeNow(info.preNow));
	checker.AddPremise(encoder.EncodeNow(info.solver.GetInstantiatedInvariant()));

	// heap remains unchanged
	checker.AddPremise(encoder.MakeForall(qvNode, encoder.EncodeTransitionMaintainsHeap(qvNode, nodeType)));

	// flow remains unchanged
	checker.AddPremise(encoder.MakeForall(qvNode, qvKey, encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)));
	checker.AddPremise(encoder.MakeForall(qvNode, encoder.EncodeNowUniqueInflow(qvNode).Implies(encoder.EncodeNextUniqueInflow(qvNode))));

	// obligations/fulfillments remain unchanged
	for (auto kind : SpecificationAxiom::KindIteratable) {
		checker.AddPremise(encoder.MakeForall(qvKey, encoder.MakeImplies(
			encoder.EncodeNowObligation(kind, qvKey), /* ==> */ encoder.EncodeNextObligation(kind, qvKey)
		)));
		for (bool value : { true, false }) {
			checker.AddPremise(encoder.MakeForall(qvKey, encoder.MakeImplies(
				encoder.EncodeNowFulfillment(kind, qvKey, value), /* ==> */ encoder.EncodeNextFulfillment(kind, qvKey, value)
			)));
		}
	}

	// ownership
	auto MaintainsOwnership = [](const VariableExpression& lhs, const Expression& rhs){
		auto [isRhsVar, rhsVar] = heal::is_of_type<VariableExpression>(rhs);
		return isRhsVar && !rhsVar->decl.is_shared && !lhs.decl.is_shared;
	};
	std::vector<Term> vector;
	for (auto [lhs, rhs] : assignment) {
		if (rhs.get().sort() != Sort::PTR) continue;
		if (MaintainsOwnership(lhs, rhs)) continue;
		auto encodedRhs = encoder.EncodeNow(rhs);
		vector.push_back(encodedRhs.Distinct(qvNode));
		checker.AddPremise(encoder.EncodeNextIsOwned(encodedRhs).Negate());
	}
	auto distinct = encoder.MakeAnd(vector);
	checker.AddPremise(encoder.MakeForall(qvNode, distinct.Implies(encoder.EncodeTransitionMaintainsOwnership(qvNode))));

	// stack
	auto IsAssignedTo = [this](const VariableDeclaration& decl) {
		for (auto [lhs, rhs] : assignment) {
			if (&lhs.get().decl == &decl) return true;
		}
		return false;
	};
	for (auto var : info.solver.GetVariablesInScope()) {
		if (IsAssignedTo(*var)) continue;
		checker.AddPremise(encoder.MakeEqual(encoder.EncodeNow(*var), encoder.EncodeNext(*var)));
	}
	for (auto [lhs, rhs] : assignment) {
		checker.AddPremise(encoder.MakeEqual(encoder.EncodeNext(lhs), encoder.EncodeNow(rhs)));

		// in case there are variable that are not covered by 'info.solver.GetVariablesInScope()'
		// TODO: properly collect variables occuring in pre and assignment
		auto [isRhsVar, rhsVar] = heal::is_of_type<VariableExpression>(rhs.get());
		if (!isRhsVar) continue;
		checker.AddPremise(encoder.MakeEqual(encoder.EncodeNow(*rhsVar), encoder.EncodeNext(*rhsVar)));
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

		auto [isVar, var] = heal::is_of_type<VariableExpression>(expr);
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
	auto result = heal::copy(predicate);

	// remove conjuncts containing lhs
	result->formula = heal::remove_conjuncts_if(std::move(result->formula), [this](const SimpleFormula& conjunct){
		for (auto [lhs, rhs] : assignment) {
			if (heal::contains_expression(conjunct, lhs)) {
				return true;
			}
		}
		return false;
	});

	return result;
}

std::unique_ptr<FuturePredicate> VarPostComputer::MakeInterimPostTime(const FuturePredicate& predicate) {
	return heal::copy(predicate);
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
		auto copy = heal::replace_expression(heal::copy(*post), transformer/*, &wasChanged*/);

		// add post and copy
		result->time.push_back(std::move(post));
		if (wasChanged) result->time.push_back(std::move(copy));
	}

	return result;
}
