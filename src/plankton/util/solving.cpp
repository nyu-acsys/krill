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

bool quick_discharge(const std::deque<const SimpleFormula*>& premises, std::deque<const SimpleFormula*>& conclusions) {
	if (premises.empty()) return conclusions.empty();
	bool result = true;
	for (auto it = conclusions.begin(); it != conclusions.end(); ++it) {
		if (syntactically_contains(premises, **it)) {
			*it = nullptr;
		} else {
			result = false;
		}
	}
	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct EncodingInfo {
	z3::context context;
	z3::solver solver;
	z3::sort ptr_sort, data_sort, sel_sort;
	z3::expr null_ptr, min_val, max_val;
	z3::func_decl heap, flow, keyset, dskeys;

	std::map<const VariableDeclaration*, z3::expr> var2expr;
	std::map<std::string, z3::expr> special2expr;
	std::map<std::pair<const Type*, std::string>, z3::expr> field2expr;
	
	int selector_count = 0;
	int temporary_count = 0;

	void initialize_solving_rules(); // TODO: make EncodingInfo a singleton to avoid reconstructing it over and over again?

	EncodingInfo() :
		solver(context),
		ptr_sort(context.int_sort()),
		data_sort(context.int_sort()),
		sel_sort(context.int_sort()),
		null_ptr(context.constant("_NULL_", ptr_sort)), 
		min_val(context.constant("_MIN_", data_sort)),
		max_val(context.constant("_MAX_", data_sort)),
		heap(context.function("$MEM", ptr_sort, sel_sort, ptr_sort)),            // free function: <address> x <selector> -> <address> + <data> + <bool>
		flow(context.function("$FL", ptr_sort, data_sort, context.bool_sort())), // free function: <address> x <data> -> <bool>
		keyset(context.function("$KS", data_sort, ptr_sort)),                    // free function: <data> -> <address>
		dskeys(context.function("$DS", data_sort, context.bool_sort()))          // free function: <data> -> <bool>
	{
		// TODO: currently null_ptr/min_val/max_valu are just some symbols that are not bound to a value
		// TODO: ptr_sort and data_sort should not be comparable??
		initialize_solving_rules();
	}

	inline z3::sort get_sort(Sort sort) {
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

	z3::expr get_var(const VariableDeclaration& decl, std::string prefix="var") {
		// TODO: ensure that no two z3 constants with the same name are created (or rely on z3 doing the renaming?)
		return get_or_create(var2expr, &decl, [this,&decl,&prefix](){
			std::string name = prefix + "$" + decl.name;
			return context.constant(name.c_str(), get_sort(decl.type.sort));
		});
	}

	z3::expr get_var(std::string name, Sort sort=Sort::BOOL) {
		// TODO: ensure that no two z3 constants with the same name are created (or rely on z3 doing the renaming?)
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

	z3::expr get_tmp(Sort sort) {
		std::string varname = "tmp$";
		switch(sort) {
			case Sort::PTR: varname += "ptr"; break;
			case Sort::DATA: varname += "data"; break;
			case Sort::BOOL: varname += "bool"; break;
			case Sort::VOID: varname += "void"; break;
		}
		varname += std::to_string(temporary_count++);
		return context.constant(varname.c_str(), get_sort(sort));
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


	template<typename T = std::vector<z3::expr>>
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
				if (plankton::config->z3_handle_unknown_result) return false;
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
		auto expr = encode(*node.expr);
		auto selector = info.get_sel(node.expr->type(), node.fieldname);
		result = info.heap(expr, selector);
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

	z3::expr mk_has_flow(const Expression& node_expr) {
		auto node = encode(node_expr);
		auto key = info.get_tmp(Sort::DATA);
		return z3::exists(key, info.flow(node, key) == info.mk_bool_val(true));
	}
	
	void visit(const OwnershipAxiom& formula) override {
		result = (info.get_var("OWNED_" + formula.expr->decl.name) == info.mk_bool_val(true));
		if (enhance_encoding) {
			result = result && !mk_has_flow(*formula.expr);
			// TODO: add "no alias"
			throw std::logic_error("not yet implemented: Encoder::visit(const OwnershipAxiom&)");

		}
	}

	void visit(const LogicallyContainedAxiom& formula) override {
		result = (info.dskeys(encode(*formula.expr)) == info.mk_bool_val(true));
	}

	void visit(const KeysetContainsAxiom& formula) override {
		result = (info.keyset(encode(*formula.value)) == encode(*formula.node));
	}

	void visit(const HasFlowAxiom& formula) override {
		result = mk_has_flow(*formula.expr);
	}

	void visit(const FlowContainsAxiom& formula) override {
		auto node = encode(*formula.expr);
		auto key = info.get_tmp(Sort::DATA);
		auto low = encode(*formula.low_value);
		auto high = encode(*formula.high_value);
		result = z3::forall(key, z3::implies((low <= key) && (key <= high), info.flow(node, key) == info.mk_bool_val(true)));
	}

	void visit(const ObligationAxiom& formula) override {
		result = (info.get_var("OBL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name) == info.mk_bool_val(true));
	}
	
	void visit(const FulfillmentAxiom& formula) override {
		result = (info.get_var("FUL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name + "_" + (formula.return_value ? "true" : "false")) == info.mk_bool_val(true));
	}

};

using encoded_outflow = std::vector<std::tuple<z3::expr, z3::expr, z3::expr, z3::expr, z3::expr>>;

inline encoded_outflow get_encoded_outflow(EncodingInfo& info) {
	Encoder encoder(info, false);
	encoded_outflow result;

	for (const auto& out : plankton::config->get_outflow_info()) {
		VariableDeclaration param_node("ptr", out.contains_key.vars.at(0)->type, false);
		VariableDeclaration param_key("key", out.contains_key.vars.at(1)->type, false);
		VariableDeclaration dummy_successor("suc", *out.node_type.field(out.fieldname), false);

		auto node = info.get_var(param_node, "qv");
		auto key = info.get_var(param_key, "qv");
		auto selector = info.get_sel(out.node_type, out.fieldname);
		auto successor = info.get_var(dummy_successor, "qv");
		auto key_flows_out = encoder.encode(*out.contains_key.instantiate(param_node, param_key));

		result.push_back(std::make_tuple(node, key, selector, successor, key_flows_out));
	}

	return result;
}

inline void add_rules_flow(EncodingInfo& info, const encoded_outflow& encoding) {
	for (auto [node, key, selector, successor, key_flows_out] : encoding) {
		// forall node,key,successor:  node->{selector}=successor /\ key in flow(node) /\ key_flows_out(node, key) ==> key in flow(successor)
		info.solver.add(z3::forall(node, key, successor, z3::implies(
			((info.heap(node, selector) == successor) && (info.flow(node, key) == info.mk_bool_val(true)) && key_flows_out),
			(info.flow(successor, key) == info.mk_bool_val(true))
		)));
	}
}

inline z3::expr make_working_iff(z3::expr premise, z3::expr conclusion) {
	// return premise == conclusion;

	// NOTE: returning an "if and only if" formula seems to break Z3.
	//       Hence, we use only an implication that---preferably the more important direction.
	return z3::implies(premise, conclusion);
}

inline void add_rules_keyset(EncodingInfo& info, const encoded_outflow& encoding) {
	// forall node,key:  key in flow(node) /\ key notin outflow(node) ==> key in keyset(node)
	auto node = std::get<0>(encoding.at(0));
	auto key = std::get<1>(encoding.at(0));

	z3::expr_vector conjuncts(info.context);
	for (auto elem : encoding) {
		auto src = info.mk_vector({ std::get<0>(elem), std::get<1>(elem) });
		auto dst = info.mk_vector({ node, key });
		auto renamed = std::get<4>(elem).substitute(src, dst);
		conjuncts.push_back(!(renamed));
	}

	info.solver.add(z3::forall(node, key, make_working_iff(
		(info.flow(node, key) == info.mk_bool_val(true)) && info.mk_and(conjuncts),
		info.keyset(key) == node
	)));

	// info.solver.add(z3::forall(node, key, z3::implies(
	// 	info.keyset(key) == node,
	// 	info.flow(node, key) == info.mk_bool_val(true)
	// )));
}

inline void add_rules_contents(EncodingInfo& info) {
	// forall key:  (exists node:  key in keyset(node) /\ contains(node, key)) ==> key in DS content
	Encoder encoder(info, false);
	auto& property = plankton::config->get_logically_contains_key();

	VariableDeclaration param_node("ptr", property.vars.at(0)->type, false);
	VariableDeclaration param_key("key", property.vars.at(1)->type, false);

	auto node = info.get_var(param_node, "qv");
	auto key = info.get_var(param_key, "qv");
	auto contains = encoder.encode(*property.instantiate(param_node, param_key));

	info.solver.add(z3::forall(key, make_working_iff(
		z3::exists(node, ((info.keyset(key) == node) && contains)),
		(info.dskeys(key) == info.mk_bool_val(true))
	)));
}

void EncodingInfo::initialize_solving_rules() {
	// enable 'model based quantifier instantiation'
	// see: https://github.com/Z3Prover/z3/blob/master/examples/c%2B%2B/example.cpp#L366
	z3::params params(context);
	params.set("mbqi", true);
	solver.set(params);

	// encode
	auto outflow_encoding = get_encoded_outflow(*this);
	if (outflow_encoding.size() == 0) {
		throw SolvingError("No flow found.");
	}

	// add rules
	add_rules_flow(*this, outflow_encoding);
	add_rules_keyset(*this, outflow_encoding);
	add_rules_contents(*this);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::deque<z3::expr> encode(EncodingInfo& info, const std::deque<const SimpleFormula*>& conjuncts, bool is_premise) {
	std::deque<z3::expr> result;
	Encoder encoder(info, is_premise);
	for (const SimpleFormula* conjunct : conjuncts) {
		if (conjunct) {
			// conjunct->accept(encoder);
			// result.push_back(is_premise ? encoder.result : !encoder.result);
			auto encoded = encoder.encode(*conjunct);
			result.push_back(is_premise ? encoded : !encoded);
		}
	}
	return result;
}

bool check_conclusions_semantically(EncodingInfo& info, const std::deque<const SimpleFormula*>& raw_conclusions) {
	auto conclusions = encode(info, raw_conclusions, false);

	if (plankton::config->implies_holistic_check) {
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
	// TODO: avoid casts
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
		throw SolvingError("Unsupported formula type for solving: unexpected subtype of 'Formula'.");
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
	auto conclusions = collect_conjuncts(implied);

	// do a quick check
	bool discharged = quick_discharge(store.raw_premises, conclusions);

	// everything implies true
	if (discharged) return true;

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

bool chk_implies(const Formula& formula, const Formula& implied) {
	// std::cout << "################# CHECK IMPLIES #################" << std::endl;
	// plankton::print(formula, std::cout);
	// std::cout << std::endl << " ====> " << std::endl;
	// plankton::print(implied, std::cout);
	// std::cout << std::endl;

	auto result = check_implication(formula, implied);
	// std::cout << std::endl << "||| chk_implies: " << (result ? "yes" : "no") << std::endl;
	// std::cout << "  - "; plankton::print(formula, std::cout); std::cout << std::endl;
	// std::cout << "  - "; plankton::print(implied, std::cout); std::cout << std::endl;
	return result;
}

bool plankton::implies(const ConjunctionFormula& formula, const ConjunctionFormula& implied) {
	return chk_implies(formula, implied);
}

bool plankton::implies(const ConjunctionFormula& formula, const SimpleFormula& implied) {
	return chk_implies(formula, implied);
}

bool plankton::implies(const SimpleFormula& formula, const ConjunctionFormula& implied) {
	return chk_implies(formula, implied);
}

bool plankton::implies(const SimpleFormula& formula, const SimpleFormula& implied) {
	return chk_implies(formula, implied);
}

bool chk_implies_nonnull(const Formula& formula, const cola::Expression& expr) {
	// TODO: find more efficient implementation that avoids copying expr
	ExpressionAxiom nonnull(std::make_unique<BinaryExpression>(BinaryExpression::Operator::NEQ, cola::copy(expr), std::make_unique<NullValue>()));
	return chk_implies(formula, nonnull);
}

bool plankton::implies_nonnull(const ConjunctionFormula& formula, const cola::Expression& expr) {
	return chk_implies_nonnull(formula, expr);
}

bool plankton::implies_nonnull(const SimpleFormula& formula, const cola::Expression& expr) {
	return chk_implies_nonnull(formula, expr);
}

bool chk_implies(const Formula& formula, const Expression& implied) {
	// TODO: find more efficient implementation that avoids copying expr
	ExpressionAxiom axiom(cola::copy(implied));
	return chk_implies(formula, axiom);
}

bool plankton::implies(const ConjunctionFormula& formula, const Expression& implied) {
	return chk_implies(formula, implied);
}

bool plankton::implies(const SimpleFormula& formula, const Expression& implied) {
	return chk_implies(formula, implied);
}

struct IterSolver final : IterativeImplicationSolver {
	ImpStore store;
	IterSolver(const Formula& premise) : store(premise) {}
	bool implies(const ConjunctionFormula& implied) override { return check_implication(store, implied); }
	bool implies(const SimpleFormula& implied) override { return check_implication(store, implied); }
};

std::unique_ptr<IterativeImplicationSolver> plankton::make_solver_from_premise(const ConjunctionFormula& premise) {
	return std::make_unique<IterSolver>(premise);
}

std::unique_ptr<IterativeImplicationSolver> plankton::make_solver_from_premise(const SimpleFormula& premise) {
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

	std::cout << "≈≈≈ unify " << annotations.size() << std::endl;
	std::cout << "################# UNIFY #################" << std::endl;
	for (const auto& annotation : annotations) {
		plankton::print(*annotation, std::cout);
		std::cout << std::endl;
	}
	std::cout << std::endl;

	if (annotations.size() == 0) return Annotation::make_true();
	if (annotations.size() == 1) return std::move(annotations.at(0));
	auto result = std::make_unique<Annotation>(unify_now(annotations), unify_time(annotations));

	std::cout << "################# UNIFY RESULT #################" << std::endl;
	plankton::print(*result, std::cout);
	std::cout << std::endl;
	return result;
}

bool plankton::implies(const Annotation& annotation, const Annotation& implied) {
	if (!plankton::implies(*annotation.now, *implied.now)) {
		return false;
	}
	for (const auto& pred : implied.time) {
		if (!annotation_implies_time(annotation, *pred)) return false;
	}
	return true;
}
