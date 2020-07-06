#include "plankton/solver/encoder.hpp"

#include <iostream> // TODO: remove
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


z3::sort Encoder::EncodeSort(Sort sort) {
	switch (sort) {
		case Sort::PTR: return intSort;
		case Sort::DATA: return intSort;
		case Sort::BOOL: return boolSort;
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
	assert(insert.second);
	return insert.first->second;
}

z3::expr Encoder::EncodeVariable(const VariableDeclaration& decl) {
	// TODO: ensure that no two z3 constants with the same name are created (or rely on z3 doing the renaming?)
	return get_or_create(decl2expr, &decl, [this,&decl](){
		std::string name = "var$" + decl.name;
		return context.constant(name.c_str(), EncodeSort(decl.type.sort));
	});
}

z3::expr Encoder::EncodeVariable(Sort sort, std::string name) {
	// TODO: ensure that no two z3 constants with the same name are created (or rely on z3 doing the renaming?)
	return get_or_create(name2expr, name, [this,&name,&sort](){
		std::string varname = "sym$" + name;
		return context.constant(varname.c_str(), EncodeSort(sort));
	});
}

z3::expr Encoder::EncodeVariable(Sort sort) {
	std::string name = "-tmp";
	switch(sort) {
		case Sort::PTR: name += "ptr"; break;
		case Sort::DATA: name += "data"; break;
		case Sort::BOOL: name += "bool"; break;
		case Sort::VOID: name += "void"; break;
	}
	name += "-" + std::to_string(temporary_count++);
	return EncodeVariable(sort, name);
}

z3::expr Encoder::EncodeSelector(selector_t selector) {
	auto [type, fieldname] = selector;
	if (!type->has_field(fieldname)) {
		throw SolvingError("Cannot encode selector: type '" + type->name + "' has no field '" + fieldname + "'.");
	}
	return get_or_create(selector2expr, std::make_pair(type, fieldname), [this](){
		return context.int_val(selector_count++);
	});
}

template<typename T = std::vector<z3::expr>>
inline z3::expr_vector mk_z3_vector(z3::context& context, T container) {
	z3::expr_vector vec(context);
	for (auto z3expr : container) {
		vec.push_back(z3expr);
	}
	return vec;
}

z3::expr_vector Encoder::EncodeVector(const std::list<z3::expr>& exprs) {
	return mk_z3_vector(context, std::move(exprs));
}

z3::expr_vector Encoder::EncodeVector(const std::deque<z3::expr>& exprs) {
	return mk_z3_vector(context, std::move(exprs));
}

z3::expr_vector Encoder::EncodeVector(const std::vector<z3::expr>& exprs) {
	return mk_z3_vector(context, std::move(exprs));
}

z3::expr Encoder::EncodeHeap(z3::expr pointer, selector_t selector) {
	return heap(pointer, EncodeSelector(selector));
}

z3::expr Encoder::EncodeFlow(z3::expr pointer, z3::expr value, bool containsValue) {
	return flow(pointer, value) == MakeBool(containsValue);
}

z3::solver Encoder::MakeSolver() {
	auto result = z3::solver(context);
	AddSolvingRulesToSolver(result);
	return result;
}

z3::expr Encoder::MakeAnd(const z3::expr_vector& conjuncts) {
	return z3::mk_and(conjuncts);
}

z3::expr Encoder::MakeAnd(const std::list<z3::expr>& conjuncts) {
	return MakeAnd(EncodeVector(std::move(conjuncts)));
}

z3::expr Encoder::MakeAnd(const std::deque<z3::expr>& conjuncts) {
	return MakeAnd(EncodeVector(std::move(conjuncts)));
}

z3::expr Encoder::MakeAnd(const std::vector<z3::expr>& conjuncts) {
	return MakeAnd(EncodeVector(std::move(conjuncts)));
}

z3::expr Encoder::MakeOr(const z3::expr_vector& disjuncts) {
	return z3::mk_or(disjuncts);
}

z3::expr Encoder::MakeOr(const std::list<z3::expr>& disjuncts) {
	return MakeOr(EncodeVector(std::move(disjuncts)));
}

z3::expr Encoder::MakeOr(const std::deque<z3::expr>& disjuncts) {
	return MakeOr(EncodeVector(std::move(disjuncts)));
}

z3::expr Encoder::MakeOr(const std::vector<z3::expr>& disjuncts) {
	return MakeOr(EncodeVector(std::move(disjuncts)));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

z3::expr Encoder::EncodePremise(const Formula& formula) {
	enhance_encoding = true;
	return EncodeInternal(formula);
}

z3::expr Encoder::EncodePremise(const Expression& expression) {
	enhance_encoding = true;
	return EncodeInternal(expression);
}

z3::expr Encoder::Encode(const Formula& formula) {
	enhance_encoding = false;
	return EncodeInternal(formula);
}

z3::expr Encoder::Encode(const Expression& expression) {
	enhance_encoding = false;
	return EncodeInternal(expression);
}


z3::expr Encoder::EncodeInternal(const Expression& expression) {
	expression.accept(*this);
	return result;
}

void Encoder::visit(const VariableDeclaration& node) {
	result = EncodeVariable(node);
}

void Encoder::visit(const BooleanValue& node) {
	result = MakeBool(node.value);
}

void Encoder::visit(const NullValue& /*node*/) {
	result = GetNullPtr();
}

void Encoder::visit(const EmptyValue& /*node*/) {
	throw SolvingError("Unsupported construct: instance of type 'EmptyValue' (aka 'EMPTY').");
}

void Encoder::visit(const MaxValue& /*node*/) {
	result = GetMaxValue();
}

void Encoder::visit(const MinValue& /*node*/) {
	result = GetMinValue();
}

void Encoder::visit(const NDetValue& /*node*/) {
	throw SolvingError("Unsupported construct: instance of type 'NDetValue' (aka '*').");
}

void Encoder::visit(const VariableExpression& node) {
	result = EncodeVariable(node.decl);
}

void Encoder::visit(const NegatedExpression& node) {
	result = !EncodeInternal(*node.expr);
}

void Encoder::visit(const BinaryExpression& node) {
	auto lhs = EncodeInternal(*node.lhs);
	auto rhs = EncodeInternal(*node.rhs);
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

void Encoder::visit(const Dereference& node) {
	auto expr = EncodeInternal(*node.expr);
	auto selector = std::make_pair(&node.type(), node.fieldname);
	result = EncodeHeap(expr, selector);
}


z3::expr Encoder::EncodeInternal(const Formula& formula) {
	formula.accept(*this);
	return result;
}

void Encoder::visit(const AxiomConjunctionFormula& formula) {
	z3::expr_vector vec(context);
	for (const auto& conjunct : formula.conjuncts) {
		vec.push_back(EncodeInternal(*conjunct));
	}
	result = MakeAnd(vec);
}

void Encoder::visit(const ConjunctionFormula& formula) {
	z3::expr_vector vec(context);
	for (const auto& conjunct : formula.conjuncts) {
		vec.push_back(EncodeInternal(*conjunct));
	}
	result = MakeAnd(vec);
}

void Encoder::visit(const NegatedAxiom& formula) {
	result = !EncodeInternal(*formula.axiom);
}

void Encoder::visit(const ExpressionAxiom& formula) {
	result = EncodeInternal(*formula.expr);
}

void Encoder::visit(const ImplicationFormula& formula) {
	bool enhance = enhance_encoding;
	enhance_encoding = !enhance;
	auto premise = EncodeInternal(*formula.premise);
	enhance_encoding = enhance;
	auto conclusion = EncodeInternal(*formula.conclusion);
	result = MakeImplication(premise, conclusion);
}

z3::expr Encoder::EncodeInternalHasFlow(const Expression& node_expr) {
	auto node = EncodeInternal(node_expr);
	auto key = EncodeVariable(Sort::DATA);
	return z3::exists(key, EncodeFlow(node, key));
}

void Encoder::visit(const OwnershipAxiom& formula) {
	z3::expr_vector conjuncts(context);

	// add special variable indicating ownership
	conjuncts.push_back(EncodeVariable(Sort::BOOL, "OWNED_" + formula.expr->decl.name) == MakeTrue());
	
	// add more knowledge about ownership that may help reasoning
	if (enhance_encoding) {
		conjuncts.push_back(!EncodeInternalHasFlow(*formula.expr)); // owned ==> no flow
		// TODO: add "no alias"
	}

	result = MakeAnd(conjuncts);
}

void Encoder::visit(const HasFlowAxiom& formula) {
	result = EncodeInternalHasFlow(*formula.expr);
}

void Encoder::visit(const FlowContainsAxiom& formula) {
	auto node = EncodeInternal(*formula.expr);
	auto key = EncodeVariable(Sort::DATA);
	auto low = EncodeInternal(*formula.low_value);
	auto high = EncodeInternal(*formula.high_value);
	result = z3::forall(key, MakeImplication((low <= key) && (key <= high), EncodeFlow(node, key)));
}

void Encoder::visit(const ObligationAxiom& formula) {
	std::string varname = "OBL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name;
	result = EncodeVariable(Sort::BOOL, varname) == MakeTrue();
}

void Encoder::visit(const FulfillmentAxiom& formula) {
	std::string varname = "FUL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name + "_" + (formula.return_value ? "true" : "false");
	result = (EncodeVariable(Sort::BOOL, varname) == MakeTrue());
}

z3::expr Encoder::EncodeInternalPredicate(const Predicate& predicate, z3::expr arg1, z3::expr arg2) {
	// we cannot instantiate 'predicate' with 'arg1' and 'arg2' directly (works only for 'VariableDeclaration's)
	auto blueprint = EncodeInternal(*predicate.blueprint);

	// get blueprint vars
	z3::expr_vector blueprint_vars(context);
	for (const auto& decl : predicate.vars) {
		blueprint_vars.push_back(EncodeVariable(*decl));
	}

	// get actual values
	z3::expr_vector replacement(context);
	replacement.push_back(arg1);
	replacement.push_back(arg2);

	// instantiate with desired values
	return blueprint.substitute(blueprint_vars, replacement);
}

z3::expr Encoder::EncodeInternalKeysetContains(z3::expr node, z3::expr key) {
	auto& outflowPredicate = postConfig.flowDomain->GetOutFlowContains();
	return EncodeFlow(node, key) && !EncodeInternalPredicate(outflowPredicate, node, key);
}

void Encoder::visit(const KeysetContainsAxiom& formula) {
	auto node = EncodeInternal(*formula.node);
	auto value = EncodeInternal(*formula.value);
	result = EncodeInternalKeysetContains(node, value);
}

void Encoder::visit(const LogicallyContainedAxiom& formula) {
	auto node = EncodeVariable(Sort::PTR);
	auto key = EncodeInternal(*formula.expr);
	auto logicallyContains = EncodeInternalPredicate(*postConfig.logicallyContainsKey, node, key);
	auto keysetContains = EncodeInternalKeysetContains(node, key);
	result = z3::exists(node, keysetContains && logicallyContains);
}
