#include "plankton/util.hpp"

#include <iostream> // TODO: remove
#include "z3++.h"
#include "cola/util.hpp"
#include "plankton/config.hpp"

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


struct EncodingInfo {
	z3::context context;
	z3::solver solver;
	z3::sort ptr_sort;
	z3::sort data_sort;
	z3::expr null_ptr;
	z3::expr min_val;
	z3::expr max_val;
	std::map<const VariableDeclaration*, z3::expr> var2expr;
	std::map<std::string, z3::expr> special2expr;

	EncodingInfo() :
		solver(context),
		ptr_sort(context.int_sort()),
		data_sort(context.int_sort()),
		null_ptr(context.constant("_NULL_", ptr_sort)), 
		min_val(context.constant("_MIN_", data_sort)),
		max_val(context.constant("_MIN_", data_sort))
	{
		// TODO: currently null_ptr/min_val/max_valu are just some symbols that are not bound to a value
		// TODO: ptr_sort and data_sort should not be comparable??
		// TODO: set up sorts (pointers, data), heap function?
	}

	z3::sort get_sort(Sort sort) {
		switch (sort) {
			case Sort::PTR: return ptr_sort;
			case Sort::DATA: return data_sort;
			case Sort::BOOL: return context.bool_sort();
			case Sort::VOID: throw SolvingError("Cannot represent cola::Sort::VOID as z3::sort.");
		}
	}

	template<typename M, typename K, typename F>
	z3::expr get_or_create(M& map, const K& key, F make_new) {
		auto find = map.find(key);
		if (find != map.end()) {
			return find->second;
		}
		auto insert = map.insert(std::make_pair(key, make_new()));
		return insert.first->second;
	}

	z3::expr get_var(const VariableDeclaration& decl) {
		return get_or_create(var2expr, &decl, [this,&decl](){
			std::string name = "var__" + decl.name;
			return context.constant(name.c_str(), get_sort(decl.type.sort));
		});
	}

	z3::expr get_var(std::string name, Sort sort=Sort::BOOL) {
		return get_or_create(special2expr, name, [this,&name,&sort](){
			std::string varname = "special__" + name;
			return context.constant(varname.c_str(), get_sort(sort));
		});
	}

	z3::expr get_nullptr() {
		return null_ptr;
	}

	z3::expr get_min_value() {
		return min_val;
	}

	z3::expr get_max_value() {
		return max_val;
	}
	
	z3::expr mk_bool_val(bool value) {
		return context.bool_val(value);
	}

	template<typename T>
	z3::expr_vector make_vector(T parts) {
		z3::expr_vector vec(context);
		for (auto z3expr : parts) {
			vec.push_back(z3expr);
		}
		return vec;
	}

	z3::expr make_and(z3::expr_vector parts) {
		return z3::mk_and(parts);
	}

	z3::expr make_or(std::deque<z3::expr> parts) {
		return z3::mk_or(make_vector(parts));
	}

	bool is_unsat() {
		auto result = solver.check();
		switch (result) {
			case z3::unknown:
				if (plankton::config.z3_handle_unknown_result) return false;
				throw SolvingError("SMT solving failed: Z3 was unable to prove SAT/UNSAT, returned 'UNKNOWN'. Cannot recover.");
			case z3::sat:
				return false;
			case z3::unsat:
				return true;
		}
	}
};

struct Encoder : public BaseVisitor, public BaseLogicVisitor {
	using BaseVisitor::visit;
	using BaseLogicVisitor::visit;

	EncodingInfo& info;
	bool is_premise;
	z3::expr result; // TODO: how to properly initialize?

	Encoder(EncodingInfo& info, bool is_premise) : info(info), is_premise(is_premise), result(info.mk_bool_val(is_premise)) {}


	z3::expr encode(const Expression& expression) {
		expression.accept(*this);
		return result;
	}

	void visit(const VariableDeclaration& node) override {
		result = info.get_var(node);
	}

	void visit(const BooleanValue& node) override {
		result = info.mk_bool_val(node.value);
	}

	void visit(const NullValue& /*node*/) override {
		result = info.get_nullptr();
	}

	void visit(const EmptyValue& /*node*/) override {
		throw SolvingError("Unsupported construct: instance of type 'EmptyValue' (aka 'EMPTY').");
	}

	void visit(const MaxValue& /*node*/) override {
		result = info.get_max_value();
	}

	void visit(const MinValue& /*node*/) override {
		result = info.get_min_value();
	}

	void visit(const NDetValue& /*node*/) override {
		throw SolvingError("Unsupported construct: instance of type 'NDetValue' (aka '*').");
	}

	void visit(const VariableExpression& node) override {
		result = info.get_var(node.decl);
	}

	void visit(const NegatedExpression& node) override {
		result = !encode(*node.expr);
	}

	void visit(const BinaryExpression& node) override {
		// TODO important: ImplicationFormula change behavior for premise/conclusion ==> we should do the same here because it could mimic an implication?
		throw std::logic_error("not yet implemented: Encoder::visit(const BinaryExpression&)");
		auto lhs = encode(*node.lhs);
		auto rhs = encode(*node.rhs);
		switch (node.op) {
			case BinaryExpression::Operator::EQ:  result = (lhs == rhs); break;
			case BinaryExpression::Operator::NEQ: result = (lhs != rhs); break;
			case BinaryExpression::Operator::LEQ: result = (lhs <= rhs); break;
			case BinaryExpression::Operator::LT:  result = (lhs < rhs);  break;
			case BinaryExpression::Operator::GEQ: result = (lhs >= rhs); break;
			case BinaryExpression::Operator::GT:  result = (lhs > rhs);  break;
			case BinaryExpression::Operator::AND: result = (lhs && rhs); break;
			case BinaryExpression::Operator::OR:  result = (lhs || rhs); break;
		}
	}

	void visit(const Dereference& /*node*/) override {
		// TODO: 'f(encode(node.expr),to_index(node.fieldname))' where f is a two-ary heap function
		throw std::logic_error("not yet implemented: Encoder::visit(const Dereference&)");
	}


	z3::expr encode(const Formula& formula) {
		formula.accept(*this);
		return result;
	}

	template<typename T>
	void do_conjunction(const T& formula) {
		z3::expr_vector vec(info.context);
		for (const auto& conjunct : formula.conjuncts) {
			vec.push_back(encode(*conjunct));
		}
		result = info.make_and(vec);
	}

	void visit(const AxiomConjunctionFormula& formula) override {
		do_conjunction(formula);
	}

	void visit(const ConjunctionFormula& formula) override {
		do_conjunction(formula);
	}

	void visit(const NegatedAxiom& formula) override {
		result = !encode(*formula.axiom);
	}

	void visit(const ExpressionAxiom& formula) override {
		result = encode(*formula.expr);
	}

	void visit(const ImplicationFormula& formula) override {
		bool was_premise = is_premise;
		is_premise = true;
		auto premise = encode(*formula.premise);
		is_premise = false;
		auto conclusion = encode(*formula.conclusion);
		is_premise = was_premise;
		result = z3::implies(premise, conclusion);
	}
	
	void visit(const OwnershipAxiom& formula) override {
		result = (info.get_var("OWNED_" + formula.expr->decl.name) == info.mk_bool_val(true));
		if (is_premise) {
			// TODO: add "no alias" and "no flow" knowledge
			throw std::logic_error("not yet implemented: Encoder::visit(const OwnershipAxiom&)");
		}
	}

	void visit(const LogicallyContainedAxiom& /*formula*/) override {
		throw std::logic_error("not yet implemented: Encoder::visit(const LogicallyContainedAxiom&)");
	}

	void visit(const KeysetContainsAxiom& /*formula*/) override {
		throw std::logic_error("not yet implemented: Encoder::visit(const KeysetContainsAxiom&)");
	}

	void visit(const FlowAxiom& /*formula*/) override {
		throw std::logic_error("not yet implemented: Encoder::visit(const FlowAxiom&)");
	}

	void visit(const ObligationAxiom& formula) override {
		result = (info.get_var("OBL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name) == info.mk_bool_val(true));
	}
	
	void visit(const FulfillmentAxiom& formula) override {
		result = (info.get_var("FUL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name + "_" + (formula.return_value ? "true" : "false")) == info.mk_bool_val(true));
	}

};

std::deque<z3::expr> encode(EncodingInfo& info, const std::deque<const SimpleFormula*>& conjuncts, bool is_premise) {
	std::deque<z3::expr> result;
	Encoder encoder(info, is_premise);
	for (const SimpleFormula* conjunct : conjuncts) {
		conjunct->accept(encoder);
		result.push_back(is_premise ? encoder.result : !encoder.result);
	}
	return result;
}

void propagate_premises(EncodingInfo& info, const std::deque<const SimpleFormula*>& raw_premises) {
	auto premises = encode(info, raw_premises, true);
	for (auto z3expr : premises) {
		info.solver.add(z3expr);
	}
}

bool check_conclusions(EncodingInfo& info, const std::deque<const SimpleFormula*>& raw_conclusions) {
	auto conclusions = encode(info, raw_conclusions, false);

	if (plankton::config.implies_holistic_check) {
		auto big_conclusion = info.make_or(std::move(conclusions));
		info.solver.add(big_conclusion);
		return info.is_unsat();

	} else {
		for (auto z3expr : conclusions) {
			info.solver.push();
			info.solver.add(z3expr);
			if (!info.is_unsat()) {
				return false;
			}
			info.solver.pop();
		}
		return true;
	}
}

bool check_implication(std::deque<const SimpleFormula*> premises, std::deque<const SimpleFormula*> conclusions) {
	static ExpressionAxiom trueFormula(std::make_unique<BooleanValue>(true));

	// do a quick check
	premises.push_back(&trueFormula);
	conclusions = quick_discharge(premises, std::move(conclusions));

	// everything implies true
	if (conclusions.empty()) return true;

	// thoroughly check implication
	EncodingInfo info;
	propagate_premises(info, premises);
	return check_conclusions(info, conclusions);
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
	std::cout << "################# UNIFY #################" << std::endl;
	for (const auto& annotation : annotations) {
		plankton::print(*annotation, std::cout);
		std::cout << std::endl;
	}
	std::cout << std::endl;

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
