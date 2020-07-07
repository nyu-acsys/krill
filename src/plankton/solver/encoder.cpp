#include "plankton/solver/encoder.hpp"

#include <iostream> // TODO: remove
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


Encoder::Encoder(const PostConfig& config) :
	intSort(context.int_sort()),
	boolSort(context.bool_sort()),
	nullPtr(context.constant("_NULL_", intSort)), 
	minVal(context.constant("_MIN_", intSort)),
	maxVal(context.constant("_MAX_", intSort)),
	heap(context.function("$MEM", intSort, intSort, intSort)),
	flow(context.function("$FLOW", intSort, intSort, boolSort)),
	ownership(context.function("$OWN", intSort, boolSort)),
	postConfig(config)
{
	// TODO: currently nullPtr/minVal/maxValu are just some symbols that are not bound to a value
}

z3::expr Encoder::GetNullPtr() {
	return nullPtr;
}
z3::expr Encoder::GetMinValue() {
	return minVal;
}
z3::expr Encoder::GetMaxValue() {
	return maxVal;
}

z3::expr Encoder::MakeBool(bool value) {
	return context.bool_val(value);
}

z3::expr Encoder::MakeTrue() {
	return MakeBool(true);
}

z3::expr Encoder::MakeFalse() {
	return MakeBool(false);
}

z3::expr Encoder::MakeImplication(z3::expr premise, z3::expr conclusion) {
	return z3::implies(premise, conclusion);
}

z3::expr Encoder::MakeAnd(const z3::expr_vector& conjuncts) {
	return z3::mk_and(conjuncts);
}

z3::expr Encoder::MakeOr(const z3::expr_vector& disjuncts) {
	return z3::mk_or(disjuncts);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

z3::sort Encoder::EncodeSort(StepTag /*tag*/, Sort sort) {
	switch (sort) {
		case Sort::PTR: return intSort;
		case Sort::DATA: return intSort;
		case Sort::BOOL: return boolSort;
		case Sort::VOID: throw EncodingError("Cannot represent cola::Sort::VOID as z3::sort.");
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

inline std::string mk_name(Encoder::StepTag tag, const VariableDeclaration& decl) {
	std::string result = tag == Encoder::NEXT ? "var-p$" : "var$";
	result += decl.name;
	return result;
}

inline std::string mk_name(Encoder::StepTag tag, std::string name) {
	std::string result = tag == Encoder::NEXT ? "sym-p$" : "sym$";
	result += name;
	return result;
}

z3::expr Encoder::EncodeVariable(StepTag tag, const VariableDeclaration& decl) {
	auto key = std::make_pair(&decl, tag);
	return get_or_create(decl2expr, key, [this,tag,&decl](){
		return context.constant(mk_name(tag, decl).c_str(), EncodeSort(tag, decl.type.sort));
	});
}

z3::expr Encoder::EncodeVariable(StepTag tag, Sort sort, std::string name) {
	auto key = std::make_pair(name, tag);
	return get_or_create(name2expr, key, [this,tag,&name,&sort](){
		return context.constant(mk_name(tag, name).c_str(), EncodeSort(tag, sort));
	});
}

z3::expr Encoder::EncodeSelector(StepTag /*tag*/, selector_t selector) {
	auto [type, fieldname] = selector;
	if (!type->has_field(fieldname)) {
		throw EncodingError("Cannot encode selector: type '" + type->name + "' has no field '" + fieldname + "'.");
	}
	auto key = std::make_pair(type, fieldname);
	return get_or_create(selector2expr, key, [this](){
		return context.int_val(selector_count++);
	});
}

z3::expr Encoder::EncodeHeap(StepTag tag, z3::expr pointer, selector_t selector) {
	if (tag == NEXT) throw std::logic_error("not yet implemented: NEXT"); // TODO: implement post heap
	return heap(pointer, EncodeSelector(tag, selector));
}

z3::expr Encoder::EncodeHeap(StepTag tag, z3::expr pointer, selector_t selector, z3::expr value) {
	return EncodeHeap(tag, pointer, selector) == value;
}

z3::expr Encoder::EncodeFlow(StepTag tag, z3::expr pointer, z3::expr value, bool containsValue) {
	if (tag == NEXT) throw std::logic_error("not yet implemented: NEXT"); // TODO: implement post flow
	return flow(pointer, value) == MakeBool(containsValue);
}

z3::expr Encoder::EncodeOwnership(StepTag tag, z3::expr pointer, bool owned) {
	if (tag == NEXT) throw std::logic_error("not yet implemented: NEXT"); // TODO: implement post ownership
	return ownership(pointer) == MakeBool(owned);
}

z3::expr Encoder::EncodeHasFlow(StepTag tag, z3::expr node) {
	auto key = EncodeVariable(tag, Sort::DATA, "qv-key"); // TODO: does this avoid name clashes?
	return z3::exists(key, EncodeFlow(tag, node, key));
}

z3::expr Encoder::EncodePredicate(StepTag tag, const Predicate& predicate, z3::expr arg1, z3::expr arg2) {
	// we cannot instantiate 'predicate' with 'arg1' and 'arg2' directly (works only for 'VariableDeclaration's)
	auto blueprint = Encode(tag, *predicate.blueprint);

	// get blueprint vars
	z3::expr_vector blueprint_vars(context);
	for (const auto& decl : predicate.vars) {
		blueprint_vars.push_back(EncodeVariable(tag, *decl));
	}

	// get actual values
	z3::expr_vector replacement(context);
	replacement.push_back(arg1);
	replacement.push_back(arg2);

	// instantiate with desired values
	return blueprint.substitute(blueprint_vars, replacement);
}

z3::expr Encoder::EncodeKeysetContains(StepTag tag, z3::expr node, z3::expr key) {
	z3::expr_vector vec(context);
	for (auto [fieldName, fieldType] : postConfig.flowDomain->GetNodeType().fields) {
		if (fieldType.get().sort != Sort::PTR) continue;
		vec.push_back(EncodePredicate(tag, postConfig.flowDomain->GetOutFlowContains(fieldName), node, key));
	}
	z3::expr keyInOutflow = MakeOr(vec);
	return EncodeFlow(tag, node, key) && !keyInOutflow;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


z3::expr Encoder::Encode(const Formula& formula, StepTag tag) {
	return EncoderCallback(*this, tag).Encode(formula);
}

z3::expr Encoder::Encode(const Expression& expression, StepTag tag) {
	return EncoderCallback(*this, tag).Encode(expression);
}


z3::expr Encoder::Encode(StepTag tag, const VariableDeclaration& node) {
	return EncodeVariable(tag, node);
}

z3::expr Encoder::Encode(StepTag /*tag*/, const BooleanValue& node) {
	return MakeBool(node.value);
}

z3::expr Encoder::Encode(StepTag /*tag*/, const NullValue& /*node*/) {
	return GetNullPtr(); // TODO: do we need a next version?
}

z3::expr Encoder::Encode(StepTag /*tag*/, const EmptyValue& /*node*/) {
	throw EncodingError("Unsupported construct: instance of type 'EmptyValue' (aka 'EMPTY').");
}

z3::expr Encoder::Encode(StepTag /*tag*/, const MaxValue& /*node*/) {
	return GetMaxValue(); // TODO: do we need a next version?
}

z3::expr Encoder::Encode(StepTag /*tag*/, const MinValue& /*node*/) {
	return GetMinValue(); // TODO: do we need a next version?
}

z3::expr Encoder::Encode(StepTag /*tag*/, const NDetValue& /*node*/) {
	throw EncodingError("Unsupported construct: instance of type 'NDetValue' (aka '*').");
}

z3::expr Encoder::Encode(StepTag tag, const VariableExpression& node) {
	return EncodeVariable(tag, node.decl);
}

z3::expr Encoder::Encode(StepTag tag, const NegatedExpression& node) {
	return !Encode(*node.expr, tag);
}

z3::expr Encoder::Encode(StepTag tag, const BinaryExpression& node) {
	auto lhs = Encode(*node.lhs, tag);
	auto rhs = Encode(*node.rhs, tag);
	switch (node.op) {
		case BinaryExpression::Operator::EQ:  return (lhs == rhs);
		case BinaryExpression::Operator::NEQ: return (lhs != rhs);
		case BinaryExpression::Operator::LEQ: return (lhs <= rhs);
		case BinaryExpression::Operator::LT:  return (lhs < rhs);
		case BinaryExpression::Operator::GEQ: return (lhs >= rhs);
		case BinaryExpression::Operator::GT:  return (lhs > rhs);
		case BinaryExpression::Operator::AND: return (lhs && rhs);
		case BinaryExpression::Operator::OR:  return (lhs || rhs);
	}
}

z3::expr Encoder::Encode(StepTag tag, const Dereference& node) {
	auto expr = Encode(*node.expr, tag);
	auto selector = std::make_pair(&node.type(), node.fieldname);
	return EncodeHeap(tag, expr, selector);
}


z3::expr Encoder::Encode(StepTag tag, const AxiomConjunctionFormula& formula) {
	z3::expr_vector vec(context);
	for (const auto& conjunct : formula.conjuncts) {
		vec.push_back(Encode(*conjunct, tag));
	}
	return MakeAnd(vec);
}

z3::expr Encoder::Encode(StepTag tag, const ConjunctionFormula& formula) {
	z3::expr_vector vec(context);
	for (const auto& conjunct : formula.conjuncts) {
		vec.push_back(Encode(*conjunct, tag));
	}
	return MakeAnd(vec);
}

z3::expr Encoder::Encode(StepTag tag, const NegatedAxiom& formula) {
	return !Encode(*formula.axiom, tag);
}

z3::expr Encoder::Encode(StepTag tag, const ExpressionAxiom& formula) {
	return Encode(*formula.expr, tag);
}

z3::expr Encoder::Encode(StepTag tag, const ImplicationFormula& formula) {
	auto premise = Encode(*formula.premise, tag);
	auto conclusion = Encode(*formula.conclusion, tag);
	return MakeImplication(premise, conclusion);
}

z3::expr Encoder::Encode(StepTag tag, const OwnershipAxiom& formula) {
	// return EncodeVariable(tag, Sort::BOOL, "OWNED_" + formula.expr->decl.name) == MakeTrue();
	return EncodeOwnership(tag, Encode(*formula.expr, tag));
}

z3::expr Encoder::Encode(StepTag tag, const HasFlowAxiom& formula) {
	return EncodeHasFlow(tag, Encode(*formula.expr, tag));
}

z3::expr Encoder::Encode(StepTag tag, const FlowContainsAxiom& formula) {
	auto node = Encode(*formula.expr, tag);
	auto key = EncodeVariable(tag, Sort::DATA, "qv-key"); // TODO: does this avoid name clashes?
	auto low = Encode(*formula.low_value, tag);
	auto high = Encode(*formula.high_value, tag);
	return z3::forall(key, MakeImplication((low <= key) && (key <= high), EncodeFlow(tag, node, key)));
}

z3::expr Encoder::Encode(StepTag tag, const ObligationAxiom& formula) {
	std::string varname = "OBL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name;
	return EncodeVariable(tag, Sort::BOOL, varname) == MakeTrue();
}

z3::expr Encoder::Encode(StepTag tag, const FulfillmentAxiom& formula) {
	std::string varname = "FUL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name + "_" + (formula.return_value ? "true" : "false");
	return (EncodeVariable(tag, Sort::BOOL, varname) == MakeTrue());
}

z3::expr Encoder::Encode(StepTag tag, const KeysetContainsAxiom& formula) {
	auto node = Encode(*formula.node, tag);
	auto value = Encode(*formula.value, tag);
	return EncodeKeysetContains(tag, node, value);
}

z3::expr Encoder::Encode(StepTag tag, const LogicallyContainedAxiom& formula) {
	auto node = EncodeVariable(tag, Sort::PTR, "qv-ptr"); // TODO: does this avoid name clashes?
	auto key = Encode(*formula.expr, tag);
	auto logicallyContains = EncodePredicate(tag, *postConfig.logicallyContainsKey, node, key);
	auto keysetContains = EncodeKeysetContains(tag, node, key);
	return z3::exists(node, keysetContains && logicallyContains);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

z3::solver Encoder::MakeSolver() {
	// create solver
	auto result = z3::solver(context);

	const Type& nodeType = postConfig.flowDomain->GetNodeType();
	for (auto tag : { NOW, NEXT }) {

		// add rule for solving flow
		// forall node,selector,successor,key:  node->{selector}=successor /\ key in flow(node) /\ key_flows_out(node, key) ==> key in flow(successor)
		for (auto [fieldName, fieldType] : nodeType.fields) {
			if (fieldType.get().sort != Sort::PTR) continue;

			auto key = EncodeVariable(tag, Sort::DATA, "qv-flow-" + fieldName + "-key");
			auto node = EncodeVariable(tag, nodeType.sort, "qv-flow-" + fieldName + "-node");
			auto successor = EncodeVariable(tag, fieldType.get().sort, "qv-flow-" + fieldName + "-successor");
			auto selector = std::make_pair(&nodeType, fieldName);

			result.add(z3::forall(node, key, successor, MakeImplication(
				EncodeHeap(tag, node, selector, successor) &&
				EncodeHasFlow(tag, node) &&
				EncodePredicate(tag, postConfig.flowDomain->GetOutFlowContains(fieldName), node, key),
				/* ==> */
				EncodeFlow(tag, successor, key)
			)));
		}

		// add rule for solving ownership
		// forall node:  owned(node) ==> !hasFlow(node)
		auto node = EncodeVariable(tag, Sort::PTR, "qv-owner-ptr");
		result.add(z3::forall(node, MakeImplication(
			EncodeOwnership(tag, node), /* ==> */ !EncodeHasFlow(tag, node)
		)));

		// add rule for solving ownership
		// forall node,other,selector:  owned(node) ==> other->{selector} != node
		for (auto [fieldName, fieldType] : nodeType.fields) {
			if (fieldType.get().sort != Sort::PTR) continue;

			auto node = EncodeVariable(tag, nodeType.sort, "qv-owner-node");
			auto other = EncodeVariable(tag, fieldType.get().sort, "qv-owner-other");
			auto selector = std::make_pair(&nodeType, fieldName);

			result.add(z3::forall(node, other, MakeImplication(
				EncodeOwnership(tag, node), /* ==> */ !EncodeHeap(tag, other, selector, node)
			)));
		}
	}

	// done
	return result;
}
