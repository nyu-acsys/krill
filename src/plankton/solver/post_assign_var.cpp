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

	transformer_t MakeNowTransformer();
	std::unique_ptr<ConjunctionFormula> MakeInterimPostNow();
	std::unique_ptr<ConjunctionFormula> MakePostNow();

	std::unique_ptr<PastPredicate> MakeInterimPostTime(const PastPredicate& predicate);
	std::unique_ptr<FuturePredicate> MakeInterimPostTime(const FuturePredicate& predicate);
	std::unique_ptr<Annotation> MakePostTime();
	transformer_t MakeTimeTransformer();
};

std::unique_ptr<Annotation> VarPostComputer::MakePost() {
	log() << std::endl << "ΩΩΩ POST for assignment: " << AssignmentString(assignment) << std::endl;
	// throw std::logic_error("point du break");
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

transformer_t VarPostComputer::MakeNowTransformer() {
	// lookup table to avoid spoinling the encoder
	static std::map<const VariableDeclaration*, std::unique_ptr<VariableDeclaration>> decl2copy;
	static std::function<const VariableDeclaration*(const VariableDeclaration&)> GetOrCreate = [](const VariableDeclaration& decl){
		auto find = decl2copy.find(&decl);
		if (find != decl2copy.end()) return find->second.get();
		auto newDecl = std::make_unique<VariableDeclaration>("post#copy-" + decl.name, decl.type, false);
		auto result = newDecl.get();
		decl2copy[&decl] = std::move(newDecl);
		return result;
	};

	// create map
	std::map<const VariableDeclaration*, const VariableDeclaration*> map;
	for (const auto& pair : assignment) {
		const VariableDeclaration* lhsVar = &pair.first.get().decl;
		assert(map.count(lhsVar) == 0);
		map[lhsVar] = GetOrCreate(*lhsVar);
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
				result.second = std::make_unique<VariableExpression>(*find->second);
			}
		}

		return result;
	};
}

std::unique_ptr<ConjunctionFormula> VarPostComputer::MakeInterimPostNow() {
	auto transformer = MakeNowTransformer();
	
	// replace left-hand side variables with dummies in 'pre'
	auto result = info.solver.AddInvariant(plankton::copy(info.preNow));
	result = plankton::replace_expression(std::move(result), transformer);

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
	auto post = info.ComputeImpliedCandidates(*checker);

	// done
	return post;
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
