#include "plankton/util.hpp"

#include <iostream> // TODO: remove
#include <list>
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

template<typename C, typename S>
bool syntactically_contains(const C& container, const S& search) {
	for (const auto& elem : container) {
		if (plankton::syntactically_equal(*elem, search)) {
			return true;
		}
	}
	return false;
}

std::deque<const SimpleFormula*> quick_discharge(const std::deque<const SimpleFormula*>& premises, std::deque<const SimpleFormula*> conclusions) {
	if (premises.empty()) {
		return conclusions;
	}
	for (const SimpleFormula*& formula : conclusions) {
		if (syntactically_contains(premises, *formula)) {
			// TODO important: does this work?
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
	z3::sort sel_sort;
	z3::expr null_ptr;
	z3::expr min_val;
	z3::expr max_val;
	z3::func_decl heap;
	std::map<const VariableDeclaration*, z3::expr> var2expr;
	std::map<std::string, z3::expr> special2expr;
	std::map<std::pair<const Type*, std::string>, z3::expr> field2expr;
	int selector_count = 0;

	EncodingInfo() :
		solver(context),
		ptr_sort(context.int_sort()),
		data_sort(context.int_sort()),
		sel_sort(context.int_sort()),
		null_ptr(context.constant("_NULL_", ptr_sort)), 
		min_val(context.constant("_MIN_", data_sort)),
		max_val(context.constant("_MAX_", data_sort)),
		heap(context.function("$MEM", ptr_sort, sel_sort, ptr_sort)) // heap: <address> x <selector> -> <address>
	{
		// TODO: currently null_ptr/min_val/max_valu are just some symbols that are not bound to a value
		// TODO: ptr_sort and data_sort should not be comparable??
		// TODO: heap function?
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
			std::string name = "var$" + decl.name;
			return context.constant(name.c_str(), get_sort(decl.type.sort));
		});
	}

	z3::expr get_var(std::string name, Sort sort=Sort::BOOL) {
		return get_or_create(special2expr, name, [this,&name,&sort](){
			std::string varname = "sym$" + name;
			return context.constant(varname.c_str(), get_sort(sort));
		});
	}

	z3::expr get_sel(const Type& type, std::string fieldname) {
		if (!type.has_field(fieldname)) {
			throw SolvingError("Cannot encode selector: type '" + type.name + "' has no field '" + fieldname + "'.");
		}
		return get_or_create(field2expr, std::make_pair(&type, fieldname), [this](){
			return context.int_val(selector_count++);
		});
	}

	z3::expr mk_deref(z3::expr expr, const Type& exprType, std::string fieldname) {
		return heap(expr, get_sel(exprType, fieldname));
	}

	z3::expr mk_nullptr() {
		return null_ptr;
	}

	z3::expr mk_min_value() {
		return min_val;
	}

	z3::expr mk_max_value() {
		return max_val;
	}
	
	z3::expr mk_bool_val(bool value) {
		return context.bool_val(value);
	}

	template<typename T>
	z3::expr_vector mk_vector(T parts) {
		z3::expr_vector vec(context);
		for (auto z3expr : parts) {
			vec.push_back(z3expr);
		}
		return vec;
	}

	z3::expr mk_and(const z3::expr_vector& parts) {
		return z3::mk_and(parts);
	}

	z3::expr mk_and(const std::deque<z3::expr>& parts) {
		return z3::mk_and(mk_vector(parts));
	}

	z3::expr mk_or(const z3::expr_vector& parts) {
		return z3::mk_or(parts);
	}

	z3::expr mk_or(const std::deque<z3::expr>& parts) {
		return z3::mk_or(mk_vector(parts));
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

	bool is_unsat(z3::expr expr) {
		solver.push();
		solver.add(expr);
		auto result = is_unsat();
		solver.pop();
		return result;
	}
};

struct Encoder : public BaseVisitor, public BaseLogicVisitor {
	using BaseVisitor::visit;
	using BaseLogicVisitor::visit;

	EncodingInfo& info;
	bool enhance_encoding;
	z3::expr result;

	Encoder(EncodingInfo& info, bool is_premise) : info(info), enhance_encoding(is_premise), result(info.mk_bool_val(is_premise)) {}


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
		result = info.mk_nullptr();
	}

	void visit(const EmptyValue& /*node*/) override {
		throw SolvingError("Unsupported construct: instance of type 'EmptyValue' (aka 'EMPTY').");
	}

	void visit(const MaxValue& /*node*/) override {
		result = info.mk_max_value();
	}

	void visit(const MinValue& /*node*/) override {
		result = info.mk_min_value();
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

	void visit(const Dereference& node) override {
		result = info.mk_deref(encode(*node.expr), node.expr->type(), node.fieldname);
	}


	z3::expr encode(const Formula& formula) {
		formula.accept(*this);
		return result;
	}

	template<typename T>
	z3::expr do_conjunction(const T& formula) {
		z3::expr_vector vec(info.context);
		for (const auto& conjunct : formula.conjuncts) {
			vec.push_back(encode(*conjunct));
		}
		return info.mk_and(vec);
	}

	void visit(const AxiomConjunctionFormula& formula) override {
		result = do_conjunction(formula);
	}

	void visit(const ConjunctionFormula& formula) override {
		result = do_conjunction(formula);
	}

	void visit(const NegatedAxiom& formula) override {
		result = !encode(*formula.axiom);
	}

	void visit(const ExpressionAxiom& formula) override {
		result = encode(*formula.expr);
	}

	void visit(const ImplicationFormula& formula) override {
		bool enhance = enhance_encoding;
		enhance_encoding = !enhance;
		auto premise = encode(*formula.premise);
		enhance_encoding = enhance;
		auto conclusion = encode(*formula.conclusion);
		result = z3::implies(premise, conclusion);
	}
	
	void visit(const OwnershipAxiom& formula) override {
		result = (info.get_var("OWNED_" + formula.expr->decl.name) == info.mk_bool_val(true));
		if (enhance_encoding) {
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

bool check_conclusions_semantically(EncodingInfo& info, const std::deque<const SimpleFormula*>& raw_conclusions) {
	auto conclusions = encode(info, raw_conclusions, false);

	if (plankton::config.implies_holistic_check) {
		auto big_conclusion = info.mk_or(std::move(conclusions));
		return info.is_unsat(big_conclusion);

	} else {
		for (auto z3expr : conclusions) {
			auto result = info.is_unsat(z3expr);
			if (!result) {
				return false;
			}
		}
		return true;
	}
}

std::deque<const SimpleFormula*> collect_conjuncts(const Formula& formula) {
	// TODO: preprocess/simplify/normalize (remove double negations, move negations to lowest level, ...)?
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

struct ImpStore {
	EncodingInfo info;
	std::deque<const SimpleFormula*> raw_premises;

	/** Constructs an 'ImpStore' for solving with a single Formula as premise.
	  * If 'negated=true' is passed, then the premise is internally *negated*
	  * in order to solve implications of the form '!premise => ...'.
	  */
	ImpStore(const Formula& premise, bool negated=false) : raw_premises(collect_conjuncts(premise)) {
		static ExpressionAxiom trueFormula(std::make_unique<BooleanValue>(true));
		raw_premises.push_back(&trueFormula);
		auto premises = encode(info, raw_premises, !negated);

		if (negated) {
			info.solver.add(info.mk_or(premises));
			raw_premises.clear(); // we must disable optimizations like quick check

		} else {
			for (auto z3expr : premises) {
				info.solver.add(z3expr);
			}
		}
	}

	/** Constructs an 'ImpStore' for solving with multiple Formulas as premise.
	  * Allows for both conjunction and disjunction of premises.
	  */
	ImpStore(std::deque<const Formula*> premises, bool conjoin_premises) {
		// if not 'conjoin_premises' we must disable quick check ==> raw_premises remain empty
		static ExpressionAxiom trueFormula(std::make_unique<BooleanValue>(true));
		if (conjoin_premises) {
			raw_premises.push_back(&trueFormula);
		}

		z3::expr_vector z3premis_vec(info.context);
		while (!premises.empty()) {
			auto conjuncts = collect_conjuncts(*premises.back());
			premises.pop_back();
			auto z3conjunct = info.mk_and(encode(info, conjuncts, true));
			z3premis_vec.push_back(z3conjunct);
			if (conjoin_premises) {
				raw_premises.insert(raw_premises.end(), conjuncts.begin(), conjuncts.end());
			}
		}

		auto z3premis = conjoin_premises ? info.mk_and(z3premis_vec) : info.mk_or(z3premis_vec);
		info.solver.add(z3premis);
	}
};

bool check_implication(ImpStore& store, const Formula& implied) {
	// do a quick check
	auto conclusions = quick_discharge(store.raw_premises, collect_conjuncts(implied));

	// everything implies true
	if (conclusions.empty()) return true;

	// thoroughly check implication
	return check_conclusions_semantically(store.info, std::move(conclusions));
}

bool check_implication_negated(ImpStore& store, const Formula& implied) {
	// note: no quick check possible
	auto conclusion = store.info.mk_and(encode(store.info, collect_conjuncts(implied), true));
	return store.info.is_unsat(conclusion);
}

bool check_implication(const Formula& formula, const Formula& implied) {
	ImpStore impStore(formula);
	return check_implication(impStore, implied);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////



bool plankton::implies(const Formula& formula, const Formula& implied) {
	// std::cout << "################# CHECK IMPLIES #################" << std::endl;
	// plankton::print(formula, std::cout);
	// std::cout << std::endl << " ====> " << std::endl;
	// plankton::print(implied, std::cout);
	// std::cout << std::endl;

	auto result = check_implication(formula, implied);
	// std::cout << " ~~> holds implication: " << (result ? "yes" : "no") << std::endl << std::endl;
	return result;
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

struct IterSolver final : IterativeImplicationSolver {
	ImpStore store;
	IterSolver(const Formula& premise) : store(premise) {}
	bool implies(const Formula& implied) override { return check_implication(store, implied); }
};

std::unique_ptr<IterativeImplicationSolver> plankton::make_solver_from_premise(const Formula& premise) {
	return std::make_unique<IterSolver>(premise);
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

struct TimeSolver {
	std::unique_ptr<ImpStore> preStore;
	std::unique_ptr<ImpStore> postStore;
	const Assignment* future_cmd;
	bool for_past;

	TimeSolver(const PastPredicate& past) : for_past(true) {
		preStore = std::make_unique<ImpStore>(*past.formula, true);
	}

	TimeSolver(const FuturePredicate& future) : future_cmd(future.command.get()), for_past(false) {
		preStore = std::make_unique<ImpStore>(*future.pre, true);
		postStore = std::make_unique<ImpStore>(*future.post);
	}

	bool implied_by(const PastPredicate& premise) {
		if (!for_past) return false;
		return check_implication_negated(*preStore, *premise.formula);
	}

	bool implied_by(const FuturePredicate& premise) {
		if (for_past) return false;
		if (!plankton::syntactically_equal(*future_cmd->lhs, *premise.command->lhs)) return false;
		if (!plankton::syntactically_equal(*future_cmd->rhs, *premise.command->rhs)) return false;
		return check_implication(*postStore, *premise.post) && check_implication_negated(*preStore, *premise.pre);
	}
};

template<typename T, typename = std::enable_if_t<std::is_base_of_v<TimePredicate, T>>>
bool annotation_implies_time(const Annotation& annotation, TimeSolver& solver) {
	for (const auto& premise : annotation.time) {
		auto check = plankton::is_of_type<T>(*premise);
		if (check.first && solver.implied_by(*check.second)) {
			return true;
		}
	}
	return false;
}

bool annotation_implies_time(const Annotation& annotation, const TimePredicate& conclusion) {
	if (const PastPredicate* past = dynamic_cast<const PastPredicate*>(&conclusion)) {
		TimeSolver solver(*past);
		return annotation_implies_time<PastPredicate>(annotation, solver);
	} else if (const FuturePredicate* future = dynamic_cast<const FuturePredicate*>(&conclusion)) {
		TimeSolver solver(*future);
		return annotation_implies_time<FuturePredicate>(annotation, solver);
	}
	throw std::logic_error("Case distinction was expected to be complete.");
}

bool all_imply(const std::vector<std::unique_ptr<Annotation>>& annotations, const TimePredicate& conclusion, const Annotation* skip=nullptr) {
	// downcast
	const PastPredicate* past = dynamic_cast<const PastPredicate*>(&conclusion);
	const FuturePredicate* future = dynamic_cast<const FuturePredicate*>(&conclusion);
	if (!past && !future) {
		throw std::logic_error("Case distinction was expected to be complete.");
	}

	// maintain solver throughout
	TimeSolver solver = past ? TimeSolver(*past) : TimeSolver(*future);

	// check predicate specific implication
	for (const auto& annotation : annotations) {
		if (annotation.get() == skip) continue;
		if (solver.for_past && annotation_implies_time<PastPredicate>(*annotation, solver)) continue;
		if (!solver.for_past && annotation_implies_time<FuturePredicate>(*annotation, solver)) continue;
		return false;
	}
	return true;
}

std::deque<std::unique_ptr<TimePredicate>> unify_time(const std::vector<std::unique_ptr<Annotation>>& annotations) {
	std::deque<std::unique_ptr<TimePredicate>> result;
	for (const auto& annotation : annotations) {
		for (const auto& pred : annotation->time) {
			if (!syntactically_contains(result, *pred)) {
				if (all_imply(annotations, *pred, annotation.get())) {
					result.push_back(plankton::copy(*pred));
				}
			}
		}
	}
	return result;
}

std::unique_ptr<ConjunctionFormula> unify_now(const std::vector<std::unique_ptr<Annotation>>& annotations) {
	// disjunction of annotations.now
	std::deque<const Formula*> nows;
	std::transform(annotations.begin(), annotations.cend(), std::back_inserter(nows), [](const auto& elem) { return elem->now.get(); });
	ImpStore now_store(nows, false);

	// copy everything that is implied by all annotations.now
	auto now_result = std::make_unique<ConjunctionFormula>();
	for (const auto& annotation : annotations) {
		for (const auto& conjunct : annotation->now->conjuncts) {
			if (!syntactically_contains(now_result->conjuncts, *conjunct)) {
				if (check_implication(now_store, *conjunct)) {
					now_result->conjuncts.push_back(plankton::copy(*conjunct));
				}
			}
		}
	}

	// done
	return now_result;
}

std::unique_ptr<Annotation> plankton::unify(std::vector<std::unique_ptr<Annotation>> annotations) {
	std::cout << "################# UNIFY #################" << std::endl;
	for (const auto& annotation : annotations) {
		plankton::print(*annotation, std::cout);
		std::cout << std::endl;
	}
	std::cout << std::endl;

	auto result = std::make_unique<Annotation>(unify_now(annotations), unify_time(annotations));

	std::cout << "################# UNIFY RESULT #################" << std::endl;
	plankton::print(*result, std::cout);
	std::cout << std::endl;
	return result;
}

bool annotation_implies(const Annotation& premise, const Annotation& conclusion) {
	if (!plankton::implies(*premise.now, *conclusion.now)) return false;
	for (const auto& pred : conclusion.time) {
		if (!annotation_implies_time(premise, *pred)) return false;
	}
	return true;
}

bool plankton::is_equal(const Annotation& annotation, const Annotation& other) {
	return annotation_implies(annotation, other) && annotation_implies(other, annotation);
}
