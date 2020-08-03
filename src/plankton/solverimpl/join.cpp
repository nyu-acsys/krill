#include "plankton/solverimpl/linsolver.hpp"

#include "plankton/logger.hpp" // TODO: remove
#include "heal/util.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


struct AnnotationJoiner {
	const SolverImpl& solver;
	std::vector<std::unique_ptr<Annotation>> annotations;
	std::vector<std::unique_ptr<ImplicationChecker>> checkers;

	AnnotationJoiner(const SolverImpl& solver, std::vector<std::unique_ptr<Annotation>> input) : solver(solver)
	{
		auto encoder = solver.GetEncoder();
		annotations.reserve(input.size());
		checkers.reserve(input.size());
		for (auto& annotation : input) {
			if (solver.ImpliesFalseQuick(*annotation)) continue;
			annotation = solver.AddInvariant(std::move(annotation));
			auto checker = solver.MakeImplicationChecker(*annotation);
			if (solver.ImpliesFalseQuick(*annotation)) continue;
			annotation->now = solver.ComputeImpliedCandidates(*checker);
			annotations.push_back(std::move(annotation));
			checkers.push_back(std::move(checker));
		}
	}

	std::unique_ptr<Annotation> MakeJoin();
	std::unique_ptr<ConjunctionFormula> MakeJoinNow();
	std::unique_ptr<Annotation> MakeJoinTimeCandidate();
	std::unique_ptr<Annotation> MakeJoinTime();
};


std::unique_ptr<Annotation> SolverImpl::Join(std::vector<std::unique_ptr<Annotation>> annotations) const {
	log() << std::endl << "∫∫∫ JOIN: " << annotations.size() << std::endl;
	auto result = AnnotationJoiner(*this, std::move(annotations)).MakeJoin();
	// log() << " ~~> " << std::endl << *result << std::endl;
	return result;
}

std::unique_ptr<Annotation> SolverImpl::Join(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) const {
	std::vector<std::unique_ptr<Annotation>> annotations;
	annotations.reserve(2);
	annotations.push_back(std::move(annotation));
	annotations.push_back(std::move(other));
	return Join(std::move(annotations));
}


std::unique_ptr<ConjunctionFormula> AnnotationJoiner::MakeJoinNow() {
	auto result = heal::copy(solver.GetCandidates());
	for (const auto& annotation : annotations) {
		result = heal::remove_conjuncts_if(std::move(result), [&annotation](const auto& conjunct) {
			return !heal::syntactically_contains_conjunct(*annotation, conjunct);
		});
	}
	return result;
}

std::unique_ptr<Annotation> AnnotationJoiner::MakeJoinTimeCandidate() {
	auto result = std::make_unique<Annotation>();
	for (const auto& annotation : annotations) {
		for (const auto& predicate : annotation->time) {
			bool contained = false;
			for (const auto& other : result->time) {
				if (heal::syntactically_equal(*predicate, *other)) {
					contained = true;
					break;
				}
			}
			if (contained) continue;
			result->time.push_back(heal::copy(*predicate));
		}
	}
	return result;
}

std::unique_ptr<Annotation> AnnotationJoiner::MakeJoinTime() {
	// get all syntactically distinct predicates from the input annotations
	auto result = MakeJoinTimeCandidate();
	auto& container = result->time;

	// remove predicates that are not implied by some (!) annotation
	for (auto& candidate : container) {
		for (const auto& checker : checkers) {
			if (checker->Implies(*candidate)) continue;
			candidate.reset();
			break;
		}
	}

	// physically remove deleted elements
	container.erase(
		std::remove_if(container.begin(), container.end(), [](const auto& uptr) { return !uptr; }),
		container.end()
	);

	// done
	return result;
}

std::unique_ptr<Annotation> AnnotationJoiner::MakeJoin() {
	// TODO: respect plankton::config->semantic_unification;
	log() << " joining " << checkers.size() << std::endl;

	switch (checkers.size()) {
		case 0: return Annotation::make_false();
		case 1: return std::move(annotations.at(0));
	}

	auto joinedNow = MakeJoinNow();
	auto result = MakeJoinTime();
	result->now = std::move(joinedNow);
	return result;
}
