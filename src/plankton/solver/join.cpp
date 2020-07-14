#include "plankton/solver/solverimpl.hpp"

#include "plankton/logger.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


struct AnnotationJoiner {
	const SolverImpl& solver;
	Encoder& encoder;
	std::vector<ImplicationCheckerImpl> checkers;
	std::vector<std::unique_ptr<Annotation>> annotations;

	AnnotationJoiner(const SolverImpl& solver, std::vector<std::unique_ptr<Annotation>> input) : solver(solver), encoder(solver.GetEncoder())
	{
		checkers.reserve(input.size());
		annotations.reserve(input.size());
		for (auto& annotation : input) {
			checkers.emplace_back(encoder, *annotation);
			annotations.push_back(std::move(annotation));
			if (!checkers.back().ImpliesFalse()) continue;
			checkers.pop_back();
			annotations.pop_back();
		}
	}

	std::unique_ptr<Annotation> MakeJoin();
	std::unique_ptr<ConjunctionFormula> MakeJoinNow();
	std::unique_ptr<Annotation> MakeJoinTimeCandidate();
	std::unique_ptr<Annotation> MakeJoinTime();
};

std::unique_ptr<Annotation> SolverImpl::Join(std::vector<std::unique_ptr<Annotation>> annotations) const {
	log() << std::endl << "∫∫∫ JOIN: " << annotations.size() << std::endl;
	return AnnotationJoiner(*this, std::move(annotations)).MakeJoin();
}


std::unique_ptr<ConjunctionFormula> AnnotationJoiner::MakeJoinNow() {
	auto result = std::make_unique<ConjunctionFormula>();
	
	for (const auto& candidate : solver.GetCandidates().conjuncts) {
		bool takeCandidate = true;

		// check if all annotations imply the candidate
		for (const auto& checker : checkers) {
			if (!checker.Implies(*candidate)) {
				takeCandidate = false;
				break;
			}
		}

		// add candidate
		if (takeCandidate) {
			result->conjuncts.push_back(plankton::copy(*candidate));
		}
	}

	return result;
}

std::unique_ptr<Annotation> AnnotationJoiner::MakeJoinTimeCandidate() {
	auto result = std::make_unique<Annotation>();
	for (const auto& annotation : annotations) {
		for (const auto& predicate : annotation->time) {
			bool contained = false;
			for (const auto& other : result->time) {
				if (plankton::syntactically_equal(*predicate, *other)) {
					contained = true;
					break;
				}
			}
			if (contained) continue;
			result->time.push_back(plankton::copy(*predicate));
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
			if (checker.Implies(*candidate)) continue;
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

	switch (checkers.size()) {
		case 0: return Annotation::make_false();
		// case 1: return std::move(annotations.at(0));
	}

	auto joinedNow = MakeJoinNow();
	auto result = MakeJoinTime();
	result->now = std::move(joinedNow);
	return result;
}
