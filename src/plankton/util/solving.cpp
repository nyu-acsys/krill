#include "plankton/util.hpp"

#include <iostream> // TODO: remove
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


struct SolvingError : public std::exception {
	const std::string cause;
	SolvingError(std::string cause_) : cause(std::move(cause_)) {}
	virtual const char* what() const noexcept { return cause.c_str(); }
};


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool syntactically_contains(const std::deque<const SimpleFormula*>& container, const SimpleFormula& search) {
	for (const auto& formula : container) {
		if (plankton::syntactically_equal(*formula, search)) {
			return true;
		}
	}
	return false;
}

std::deque<const SimpleFormula*> quick_discharge(const std::deque<const SimpleFormula*>& premises, std::deque<const SimpleFormula*> conclusions) {
	for (const SimpleFormula*& formula : conclusions) {
		if (syntactically_contains(premises, *formula)) {
			// TODO: does this work?
			formula = conclusions.back();
			conclusions.back() = nullptr;
		}
		// TODO: add more heuristics
	}
	while (!conclusions.empty() && conclusions.back() == nullptr) {
		conclusions.pop_back();
	}
	return conclusions;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


bool check_implication(std::deque<const SimpleFormula*> premises, std::deque<const SimpleFormula*> conclusions) {
	static ExpressionAxiom trueFormula(std::make_unique<BooleanValue>(true));

	// do a quick check
	premises.push_back(&trueFormula);
	conclusions = quick_discharge(premises, std::move(conclusions));

	// everything implies true
	if (conclusions.empty()) return true;

	// thoroughly check implication
	throw std::logic_error("not yet implemented (check_implication)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::deque<const SimpleFormula*> collect_conjuncts(const Formula& formula) {
	// TODO: preprocess/simplify (remove double negations)?
	std::deque<const SimpleFormula*> result;

	if (const SimpleFormula* form = dynamic_cast<const SimpleFormula*>(&formula)) {
		result.push_back(form);

	} else if (const ConjunctionFormula* form = dynamic_cast<const ConjunctionFormula*>(&formula)) {
		for (const auto& conjunct : form->conjuncts) {
			result.push_back(conjunct.get());
		}

	} else if (const AxiomConjunctionFormula* form = dynamic_cast<const AxiomConjunctionFormula*>(&formula)) {
		for (const auto& conjunct : form->conjuncts) {
			result.push_back(conjunct.get());
		}

	} else {
		throw SolvingError("Unsupported formula type for solving: '" + std::string(typeid(Formula).name()) + "'.");
	}

	return result;
}

bool plankton::implies(const Formula& formula, const Formula& implied) {
	std::cout << "################# CHECK IMPLIES #################" << std::endl;
	plankton::print(formula, std::cout);
	std::cout << std::endl << " ====>> " << std::endl;
	plankton::print(implied, std::cout);
	std::cout << std::endl << std::endl;

	return check_implication(collect_conjuncts(formula), collect_conjuncts(implied));
}

bool plankton::implies_nonnull(const Formula& formula, const cola::Expression& expr) {
	// TODO: find more efficient implementation that avoids copying expr
	ExpressionAxiom nonnull(std::make_unique<BinaryExpression>(BinaryExpression::Operator::NEQ, cola::copy(expr), std::make_unique<NullValue>()));
	return plankton::implies(formula, nonnull);
}

bool plankton::implies(const Formula& formula, const Expression& implied) {
	// TODO: find more efficient implementation that avoids copying expr
	ExpressionAxiom axiom(cola::copy(implied));
	return plankton::implies(formula, axiom);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> plankton::unify(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) {
	std::vector<std::unique_ptr<Annotation>> vec;
	vec.push_back(std::move(annotation));
	vec.push_back(std::move(other));
	return plankton::unify(std::move(vec));
}

template<typename T>
struct PieceIterator {
	using container_t = const std::vector<std::unique_ptr<Annotation>>&;
	using outer_iterator_t = std::vector<std::unique_ptr<Annotation>>::const_iterator;
	using inner_iterator_t = typename std::deque<std::unique_ptr<T>>::const_iterator;
	using inner_iterator_getter_t = std::function<inner_iterator_t(const Annotation&)>;

	container_t container;
	outer_iterator_t current_outer;
	inner_iterator_t current_inner;
	inner_iterator_getter_t get_begin;
	inner_iterator_getter_t get_end;

	virtual inner_iterator_t inner_begin() const { return get_begin(*current_outer->get()); }
	virtual inner_iterator_t inner_end() const { return get_end(*current_outer->get()); }

	virtual void progress_to_next() {
		while (current_outer != container.end()) {
			current_inner = inner_begin();
			if (current_inner != inner_end()) {
				break;
			}
			++current_outer;
		}
	}

	virtual ~PieceIterator() = default;
	PieceIterator(container_t container_, inner_iterator_getter_t begin, inner_iterator_getter_t end) : container(container_), get_begin(begin), get_end(end) {
		current_outer = container.begin();
		progress_to_next();
	}

	bool has_next() {
		return current_outer != container.end();
	}

	std::pair<const Annotation*, const T*> get_next() {
		auto result = current_inner->get();
		++current_inner;
		if (current_inner == inner_end()) {
			++current_outer;
			progress_to_next();
		}
		return std::make_pair(current_outer->get(), result);
	}
};

PieceIterator<SimpleFormula> iterator_for_simple_formulas(const std::vector<std::unique_ptr<Annotation>>& container) {
	return PieceIterator<SimpleFormula>(
		container,
		[](const auto& annotation){ return annotation.now->conjuncts.begin(); },
		[](const auto& annotation){ return annotation.now->conjuncts.end(); }
	);
}

PieceIterator<TimePredicate> iterator_for_time_predicates(const std::vector<std::unique_ptr<Annotation>>& container) {
	return PieceIterator<TimePredicate>(
		container,
		[](const auto& annotation){ return annotation.time.begin(); },
		[](const auto& annotation){ return annotation.time.end(); }
	);
}

bool time_implies(const PastPredicate& premise, const PastPredicate& conclusion) {
	return plankton::implies(*premise.formula, *conclusion.formula);
}

bool time_implies(const FuturePredicate& premise, const FuturePredicate& conclusion) {
	return plankton::implies(*premise.pre, *conclusion.pre) && plankton::implies(*conclusion.post, *premise.post);
}

template<typename T, typename = std::enable_if_t<std::is_base_of_v<TimePredicate, T>>>
bool annotation_implies_time(const Annotation& annotation, const T& conclusion) {
	for (const auto& premise : annotation.time) {
		auto check = plankton::is_of_type<T>(*premise);
		if (check.first && time_implies(*check.second, conclusion)) {
			return true;
		}
	}
	return false;
}

bool annotation_implies(const Annotation& annotation, const Formula& formula) {
	if (const SimpleFormula* form = dynamic_cast<const SimpleFormula*>(&formula)) {
		return plankton::implies(*annotation.now, *form);

	} else if (const PastPredicate* pred = dynamic_cast<const PastPredicate*>(&formula)) {
		return annotation_implies_time(annotation, *pred);

	} else if (const FuturePredicate* pred = dynamic_cast<const FuturePredicate*>(&formula)) {
		return annotation_implies_time(annotation, *pred);

	} else if (const ConjunctionFormula* con = dynamic_cast<const ConjunctionFormula*>(&formula)) {
		return plankton::implies(*annotation.now, *con);
	}

	std::string name(typeid(formula).name());
	throw std::logic_error("Case distinction was supposed to be complete, unexpected type with typeid '" + name + "'.");
}

template<typename T>
bool all_imply(const T& container, const Formula& conclusion, const Annotation* skip=nullptr) {
	for (const auto& elem : container) {
		const Annotation& annotation = *elem;
		if (&annotation == skip) continue;
		if (!annotation_implies(annotation, conclusion)) {
			return false;
		}
	}
	return true;
}

std::unique_ptr<Annotation> plankton::unify(std::vector<std::unique_ptr<Annotation>> annotations) {
	auto result = std::make_unique<Annotation>();

	auto now_iterator = iterator_for_simple_formulas(annotations);
	while (now_iterator.has_next()) {
		auto [annotation, formula] = now_iterator.get_next();
		if (all_imply(annotations, *formula, annotation)) {
			result->now->conjuncts.push_back(plankton::copy(*formula));
		}
	}

	auto time_iterator = iterator_for_time_predicates(annotations);
	while (time_iterator.has_next()) {
		auto [annotation, pred] = time_iterator.get_next();
		if (all_imply(annotations, *pred, annotation)) {
			result->time.push_back(plankton::copy(*pred));
		}
	}

	return result;
}

bool annotation_implies(const Annotation& premise, const Annotation& conclusion) {
	if (!plankton::implies(*premise.now, *conclusion.now)) return false;
	if (!plankton::implies(*conclusion.now, *premise.now)) return false;
	for (const auto& pred : conclusion.time) {
		if (!annotation_implies(premise, *pred)) return false;
	}
	for (const auto& pred : premise.time) {
		if (!annotation_implies(conclusion, *pred)) return false;
	}
	return true;
}

bool plankton::is_equal(const Annotation& annotation, const Annotation& other) {
	return annotation_implies(annotation, other) && annotation_implies(other, annotation);
}
