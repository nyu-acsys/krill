#include "plankton/verify.hpp"

#include <map>
#include "cola/util.hpp"
#include "plankton/config.hpp"
#include "plankton/logger.hpp" // TODO: delete

using namespace cola;
using namespace heal;
using namespace plankton;


bool commands_equal(const Assignment& cmd, const Assignment& other) {
	static std::map<std::pair<std::size_t, std::size_t>, bool> lookup;

	std::pair<std::size_t, std::size_t> key;
	key = cmd.id <= other.id ? std::make_pair(cmd.id, other.id) : std::make_pair(other.id, cmd.id);

	auto find = lookup.find(key);
	if (find != lookup.end()) {
		return find->second;

	} else {
		bool result = heal::syntactically_equal(*cmd.lhs, *other.lhs) && heal::syntactically_equal(*cmd.rhs, *other.rhs);
		lookup[key] = result;
		return result;
	}
}

void Verifier::extend_interference(std::unique_ptr<Effect> effect) {
	return; // TODO important: reenable <<<<<<<<<<<=====================================================================||||||||||||

	bool is_effect_new = true;
	bool effect_pruned = false;

	// check if we already covered the new effect; delete effects covered by the new one
	for (auto& other : interference) {
		if (!commands_equal(*effect->command, *other->command)) {
			continue;
		}

		// TODO: implement iterative solving => extend effects with an ImplicationChecker?
		if (solver->Implies(*effect->precondition, *other->precondition)) {
			is_effect_new = false;
			break;
		}
		if (solver->Implies(*other->precondition, *effect->precondition)) {
			other.reset();
			effect_pruned = true;
		}
	}

	// remove pruned effects
	if (effect_pruned) {
		std::deque<std::unique_ptr<Effect>> new_interference;
		for (auto& other : interference) {
			if (other) {
				new_interference.push_back(std::move(other));
			}
		}
		interference = std::move(new_interference);
	}

	// add new effect if needed
	if (is_effect_new) {
		interference.push_back(std::move(effect));
	}
}

void Verifier::extend_interference(const Assignment& command) {
	current_annotation = solver->AddInvariant(std::move(current_annotation));
	auto transformer = interference_renaming_info.as_transformer();

	// make effect: (rename(current_annotation.now), rename(command)) where rename(x) renames the local variables in x
	auto pre = heal::replace_expression(heal::copy(*current_annotation->now), transformer);
	auto cmd = std::make_unique<Assignment>(
		heal::replace_expression(cola::copy(*command.lhs), transformer),
		heal::replace_expression(cola::copy(*command.rhs), transformer)
	);
	auto effect = std::make_unique<Effect>(std::move(pre), std::move(cmd));

	// extend interference
	extend_interference(std::move(effect));
}


void Verifier::apply_interference() {
	return; // TODO important: reenable <<<<<<<<<<<=====================================================================||||||||||||
	if (inside_atomic) return;

	auto stable = std::make_unique<ConjunctionFormula>();
	current_annotation = solver->StripInvariant(std::move(current_annotation));
	log() << std::endl << "∆∆∆ applying interference" << std::endl;
	std::size_t counter = 0;

	if (solver->ImpliesFalse(*current_annotation)) { // TODO: do a quick/syntactic check
		log() << "    => pruning: premise is false" << std::endl;
		return;
	}

	// quick check
	// TODO: remove SpecificationAxioms and add them back at the end
	for (auto& conjunct : current_annotation->now->conjuncts) {
		if (!has_effect(*conjunct)) {
			stable->conjuncts.push_back(std::move(conjunct));
			conjunct.reset();
		}
	}
	// log() << "    => after quick check: " << *stable << std::endl;

	// deep check
	bool changed;
	do {
		changed = false;
		for (auto& conjunct : current_annotation->now->conjuncts) {
			if (!conjunct) continue;
			++counter;

			// add conjunct to stable
			stable->conjuncts.push_back(std::move(conjunct));
			conjunct.reset();

			// check if stable is still interference free; if not, move conjunct back again
			if (!is_interference_free(*stable)) {
				conjunct = std::move(stable->conjuncts.back());
				stable->conjuncts.pop_back();

			} else {
				changed = true;
			}
		}
	} while(changed && plankton::config.interference_exhaustive_repetition);

	// TODO important: take non-interference-free stuff and (1) see if an implication helps to make it interference-free, (2) put it into a Hist

	// keep interference-free part
	current_annotation->now = std::move(stable);

	log() << "    => number of interference freedom checks: " << counter << std::endl;
}


bool Verifier::is_interference_free(const ConjunctionFormula& formula){
	for (const auto& effect : interference) {
		// TODO important: should we combine effect->precondition and formula?
		if (!solver->UncheckedPostEntails(*effect->precondition, *effect->command, formula)) {
			// log() << " ==> no" << std::endl;
			return false;
		}
	}
	return true;
}
