#include "plankton/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


bool plankton::implies_nonnull(const Formula& /*formula*/, const cola::Expression& /*expr*/) {
	throw std::logic_error("not yet implemented (plankton::implies_nonnull)");
	// TODO: find more efficient implementation that avoids copying expr
	// BinaryExpression nonnull(BinaryExpression::Operator::NEQ, cola::copy(expr), std::make_unique<NullValue>());
	// return plankton::implies(formula, nonnull);
}

bool plankton::implies(const Formula& /*formula*/, const Expression& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
	// TODO: find more efficient implementation that avoids copying expr
	// ExpressionAxiom axiom(cola::copy(implied));
	// return plankton::implies(formula, axiom);
}

bool plankton::implies(const Formula& /*formula*/, const Formula& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::implies(const Formula& /*formula*/, const std::vector<std::reference_wrapper<const Formula>>& /*implied*/) {
	throw std::logic_error("not yet implemented (plankton::implies)");
}

bool plankton::is_equal(const Annotation& /*annotation*/, const Annotation& /*other*/) {
	throw std::logic_error("not yet implemented (plankton::is_equal)");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool check_implication(const Formula& /*premise*/, const Formula& /*otherPremise*/, const Formula& /*conclusion*/) {
	throw std::logic_error("not yet implemented (check_implication)");
}

bool check_implication(const Formula& premise, const Formula& conclusion) {
	static ExpressionAxiom trueFormula(std::make_unique<BooleanValue>(true));
	return check_implication(premise, trueFormula, conclusion);
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
	using outer_iterator_t = std::vector<std::unique_ptr<Annotation>>::const_iterator;
	using inner_iterator_t = typename std::deque<std::unique_ptr<T>>::const_iterator;

	const std::vector<std::unique_ptr<Annotation>>& container;
	outer_iterator_t current_outer;
	inner_iterator_t current_inner;

	virtual inner_iterator_t inner_begin() const = 0;
	virtual inner_iterator_t inner_end() const = 0;

	void progress_to_next() {
		while (current_outer != container.end()) {
			current_inner = inner_begin();
			if (current_inner != inner_end()) {
				break;
			}
			++current_outer;
		}
	}

	virtual ~PieceIterator() = default;
	PieceIterator(const std::vector<std::unique_ptr<Annotation>>& container_) : container(container_) {
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

struct NowIterator : public PieceIterator<SimpleFormula> {
	inner_iterator_t inner_begin() const override { return (*current_outer)->now->conjuncts.begin(); }
	inner_iterator_t inner_end() const override { return (*current_outer)->now->conjuncts.end(); }

	NowIterator(const std::vector<std::unique_ptr<Annotation>>& container) : PieceIterator(container) {}
};

struct TimeIterator : public PieceIterator<TimePredicate> {
	inner_iterator_t inner_begin() const override { return (*current_outer)->time.begin(); }
	inner_iterator_t inner_end() const override { return (*current_outer)->time.end(); }

	TimeIterator(const std::vector<std::unique_ptr<Annotation>>& container) : PieceIterator(container) {}
};

bool annotation_implies(const Annotation& annotation, const Axiom& formula) {
	return check_implication(*annotation.now, formula);
}

bool annotation_implies(const Annotation& annotation, const ImplicationFormula& formula) {
	return check_implication(*annotation.now, *formula.premise, *formula.conclusion);
}

bool time_implies(const PastPredicate& premise, const PastPredicate& conclusion) {
	return check_implication(*premise.formula, *conclusion.formula);
}

bool time_implies(const FuturePredicate& premise, const FuturePredicate& conclusion) {
	return check_implication(*premise.pre, *conclusion.pre) && check_implication(*conclusion.post, *premise.post);
}

template<typename T, typename = std::enable_if_t<std::is_base_of_v<TimePredicate, T>>>
bool annotation_implies(const Annotation& annotation, const T& conclusion) {
	for (const auto& premise : annotation.time) {
		auto check = plankton::is_of_type<T>(*premise);
		if (check.first && time_implies(*check.second, conclusion)) {
			return true;
		}
	}
	return false;
}

bool annotation_implies(const Annotation& annotation, const Formula& formula) {
	if (const Axiom* axiom = dynamic_cast<const Axiom*>(&formula)) {
		return annotation_implies(annotation, *axiom);

	} else if (const ImplicationFormula* imp = dynamic_cast<const ImplicationFormula*>(&formula)) {
		return annotation_implies(annotation, *imp);

	} else if (const PastPredicate* pred = dynamic_cast<const PastPredicate*>(&formula)) {
		return annotation_implies<PastPredicate>(annotation, *pred);

	} else if (const FuturePredicate* pred = dynamic_cast<const FuturePredicate*>(&formula)) {
		return annotation_implies<FuturePredicate>(annotation, *pred);
	}

	std::string name(typeid(formula).name());
	throw std::logic_error("Case distinction was supposed to be complete, unexpected type with typeid '" + name + "'.");
}

bool all_imply(const std::vector<std::unique_ptr<Annotation>>& annotations, const Formula& conclusion, const Annotation* skip=nullptr) {
	for (const auto& annotation : annotations) {
		if (annotation.get() == skip) continue;
		if (!annotation_implies(*annotation, conclusion)) {
			return false;
		}
	}
	return true;
}

std::unique_ptr<Annotation> plankton::unify(std::vector<std::unique_ptr<Annotation>> annotations) {
	auto result = std::make_unique<Annotation>();

	NowIterator now_iterator(annotations);
	while (now_iterator.has_next()) {
		auto [annotation, formula] = now_iterator.get_next();
		if (all_imply(annotations, *formula, annotation)) {
			result->now->conjuncts.push_back(plankton::copy(*formula));
		}
	}

	TimeIterator time_iterator(annotations);
	while (time_iterator.has_next()) {
		auto [annotation, pred] = time_iterator.get_next();
		if (all_imply(annotations, *pred, annotation)) {
			result->time.push_back(plankton::copy(*pred));
		}
	}

	return result;
}
