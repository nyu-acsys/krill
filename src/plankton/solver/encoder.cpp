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
	heapNow(context.function("$MEM-now", intSort, intSort, intSort)),
	heapNext(context.function("$MEM-next", intSort, intSort, intSort)),
	flowNow(context.function("$FLOW-now", intSort, intSort, boolSort)),
	flowNext(context.function("$FLOW-next", intSort, intSort, boolSort)),
	ownershipNow(context.function("$OWN-now", intSort, boolSort)),
	ownershipNext(context.function("$OWN-next", intSort, boolSort)),
	obligationNow(context.function("$OBL-now", intSort, intSort, boolSort)), // TODO: second paramter should be a finite sort
	obligationNext(context.function("$OBL-next", intSort, intSort, boolSort)), // TODO: second paramter should be a finite sort
	fulfillmentNow(context.function("$FUL-now", intSort, intSort, boolSort, boolSort)), // TODO: second paramter should be a finite sort
	fulfillmentNext(context.function("$FUL-next", intSort, intSort, boolSort, boolSort)), // TODO: second paramter should be a finite sort
	postConfig(config)
{
	// TODO: currently nullPtr/minVal/maxValu are just some symbols that are not bound to a value
}

const std::vector<std::pair<std::string, const Type*>>& Encoder::GetNodeTypePointerFields() { // TODO: needed? remove?
	if (!pointerFields.has_value()) {
		std::vector<std::pair<std::string, const Type*>> result;
		for (auto [fieldName, fieldType] : postConfig.flowDomain->GetNodeType().fields) {
			if (fieldType.get().sort != Sort::PTR) continue;
			result.push_back(std::make_pair(fieldName, &fieldType.get()));
		}
		pointerFields = std::move(result);
	}
	return pointerFields.value();
}

z3::expr Encoder::MakeNullPtr() {
	return nullPtr;
}
z3::expr Encoder::MakeMinValue() {
	return minVal;
}
z3::expr Encoder::MakeMaxValue() {
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

z3::expr Encoder::MakeEquivalence(z3::expr expression, z3::expr other) {
	// although equivalent, both versions behave differently (performance-wise)
	return expression == other;
	// return (expression && other) || (!expression && !other);
}

z3::expr Encoder::MakeAnd(const z3::expr_vector& conjuncts) {
	return z3::mk_and(conjuncts);
}

z3::expr Encoder::MakeOr(const z3::expr_vector& disjuncts) {
	return z3::mk_or(disjuncts);
}

z3::expr_vector Encoder::MakeVector() {
	return z3::expr_vector(context);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

z3::sort Encoder::EncodeSort(Sort sort) {
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
	std::string result = tag == Encoder::StepTag::NEXT ? "var-p$" : "var$";
	result += decl.name;
	return result;
}

inline std::string mk_name(Encoder::StepTag tag, std::string name) {
	std::string result = tag == Encoder::StepTag::NEXT ? "sym-p$" : "sym$";
	result += name;
	return result;
}

z3::expr Encoder::EncodeVariable(const VariableDeclaration& decl, StepTag tag) {
	auto key = std::make_pair(&decl, tag);
	return get_or_create(decl2expr, key, [this,tag,&decl](){
		return context.constant(mk_name(tag, decl).c_str(), EncodeSort(decl.type.sort));
	});
}

z3::expr Encoder::EncodeVariable(Sort sort, std::string name, StepTag tag) {
	auto key = std::make_pair(name, tag);
	return get_or_create(name2expr, key, [this,tag,&name,&sort](){
		return context.constant(mk_name(tag, name).c_str(), EncodeSort(sort));
	});
}

z3::expr Encoder::EncodeSelector(selector_t selector, StepTag /*tag*/) {
	auto [type, fieldname] = selector;
	if (!type->has_field(fieldname)) {
		throw EncodingError("Cannot encode selector: type '" + type->name + "' has no field '" + fieldname + "'.");
	}
	auto key = std::make_pair(type, fieldname);
	return get_or_create(selector2expr, key, [this](){
		return context.int_val(selector_count++);
	});
}

z3::expr Encoder::EncodeHeap(z3::expr pointer, selector_t selector, StepTag tag) {
	switch (tag) {
		case StepTag::NOW: return heapNow(pointer, EncodeSelector(selector, tag));
		case StepTag::NEXT: return heapNext(pointer, EncodeSelector(selector, tag));
	}
}

z3::expr Encoder::EncodeHeap(z3::expr pointer, selector_t selector, z3::expr value, StepTag tag) {
	return EncodeHeap(pointer, selector, tag) == value;
}

z3::expr Encoder::EncodeFlow(z3::expr pointer, z3::expr value, bool containsValue, StepTag tag) {
	switch (tag) {
		case StepTag::NOW: return flowNow(pointer, value) == MakeBool(containsValue);
		case StepTag::NEXT: return flowNext(pointer, value) == MakeBool(containsValue);
	}
}

z3::expr Encoder::EncodeOwnership(z3::expr pointer, bool owned, StepTag tag) {
	switch (tag) {
		case StepTag::NOW: return ownershipNow(pointer) == MakeBool(owned);
		case StepTag::NEXT: return ownershipNext(pointer) == MakeBool(owned);
	}
}

z3::expr Encoder::EncodeHasFlow(z3::expr node, StepTag tag) {
	auto key = EncodeVariable(Sort::DATA, "qv-key", tag); // TODO: does this avoid name clashes?
	return z3::exists(key, EncodeFlow(node, key, true, tag));
}

z3::expr Encoder::EncodeSpec(SpecificationAxiom::Kind kind) {
	switch (kind) {
		case SpecificationAxiom::Kind::CONTAINS: return context.int_val(0);
		case SpecificationAxiom::Kind::INSERT: return context.int_val(1);
		case SpecificationAxiom::Kind::DELETE: return context.int_val(2);
	}
}

z3::expr Encoder::EncodeObligation(SpecificationAxiom::Kind kind, z3::expr key, StepTag tag) {
	switch (tag) {
		case StepTag::NOW: return obligationNow(key, EncodeSpec(kind)) == MakeTrue();
		case StepTag::NEXT: return obligationNext(key, EncodeSpec(kind)) == MakeTrue();
	}
}

z3::expr Encoder::EncodeFulfillment(SpecificationAxiom::Kind kind, z3::expr key, bool returnValue, StepTag tag) {
	switch (tag) {
		case StepTag::NOW: return fulfillmentNow(key, EncodeSpec(kind), MakeBool(returnValue)) == MakeTrue();
		case StepTag::NEXT: return fulfillmentNext(key, EncodeSpec(kind), MakeBool(returnValue)) == MakeTrue();
	}
}

template<PropertyArity N, typename T>
inline z3::expr EncodeProperty(Encoder& encoder, z3::context& context, const Property<N, T>& property, std::vector<z3::expr> args, Encoder::StepTag tag) {
	// we cannot instantiate 'property' with 'args' directly (works only for 'VariableDeclaration's)
	auto blueprint = encoder.Encode(*property.blueprint, tag);

	// get blueprint vars
	z3::expr_vector blueprint_vars(context);
	for (const auto& decl : property.vars) {
		blueprint_vars.push_back(encoder.EncodeVariable(*decl, tag));
	}

	// get actual values
	z3::expr_vector replacement(context);
	for (auto arg : args) {
		replacement.push_back(arg);
	}

	// instantiate with desired values
	assert(blueprint_vars.size() == replacement.size());
	return blueprint.substitute(blueprint_vars, replacement);
}

z3::expr Encoder::EncodeInvariant(const Invariant& invariant, z3::expr arg, StepTag tag) {
	return EncodeProperty(*this, context, invariant, { arg }, tag);
}

z3::expr Encoder::EncodePredicate(const Predicate& predicate, z3::expr arg1, z3::expr arg2, StepTag tag) {
	return EncodeProperty(*this, context, predicate, { arg1, arg2 }, tag);
}

z3::expr Encoder::EncodeKeysetContains(z3::expr node, z3::expr key, StepTag tag) {
	z3::expr_vector vec(context);
	for (auto [fieldName, fieldType] : postConfig.flowDomain->GetNodeType().fields) {
		if (fieldType.get().sort != Sort::PTR) continue;
		vec.push_back(EncodePredicate(postConfig.flowDomain->GetOutFlowContains(fieldName), node, key, tag));
	}
	z3::expr keyInOutflow = MakeOr(vec);
	return EncodeFlow(node, key, true, tag) && !keyInOutflow;
}

z3::expr Encoder::EncodeTransitionMaintainsHeap(z3::expr node, const Type& nodeType, std::set<std::string> excludedSelectors) {
	z3::expr_vector vec(context);
	for (auto [fieldName, fieldType] : nodeType.fields) {
		if (excludedSelectors.count(fieldName) != 0) continue;
		auto selector = std::make_pair(&nodeType, fieldName);
		vec.push_back(EncodeHeap(node, selector) == EncodeNextHeap(node, selector));
	}
	return MakeAnd(vec);
}

z3::expr Encoder::EncodeTransitionMaintainsFlow(z3::expr node, z3::expr key) {
	return MakeEquivalence(EncodeFlow(node, key), EncodeNextFlow(node, key));
}

z3::expr Encoder::EncodeTransitionMaintainsOwnership(z3::expr node) {
	return MakeEquivalence(EncodeOwnership(node), EncodeNextOwnership(node));
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
	return EncodeVariable(node, tag);
}

z3::expr Encoder::Encode(StepTag /*tag*/, const BooleanValue& node) {
	return MakeBool(node.value);
}

z3::expr Encoder::Encode(StepTag /*tag*/, const NullValue& /*node*/) {
	return MakeNullPtr(); // TODO: do we need a next version?
}

z3::expr Encoder::Encode(StepTag /*tag*/, const EmptyValue& /*node*/) {
	throw EncodingError("Unsupported construct: instance of type 'EmptyValue' (aka 'EMPTY').");
}

z3::expr Encoder::Encode(StepTag /*tag*/, const MaxValue& /*node*/) {
	return MakeMaxValue(); // TODO: do we need a next version?
}

z3::expr Encoder::Encode(StepTag /*tag*/, const MinValue& /*node*/) {
	return MakeMinValue(); // TODO: do we need a next version?
}

z3::expr Encoder::Encode(StepTag /*tag*/, const NDetValue& /*node*/) {
	throw EncodingError("Unsupported construct: instance of type 'NDetValue' (aka '*').");
}

z3::expr Encoder::Encode(StepTag tag, const VariableExpression& node) {
	return EncodeVariable(node.decl, tag);
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
	auto selector = std::make_pair(&node.expr->type(), node.fieldname);
	return EncodeHeap(expr, selector, tag);
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
	return EncodeOwnership(Encode(*formula.expr, tag), true, tag);
}

z3::expr Encoder::Encode(StepTag tag, const HasFlowAxiom& formula) {
	return EncodeHasFlow(Encode(*formula.expr, tag), tag);
}

z3::expr Encoder::Encode(StepTag tag, const FlowContainsAxiom& formula) {
	auto node = Encode(*formula.expr, tag);
	auto key = EncodeVariable(Sort::DATA, "qv-key", tag); // TODO: does this avoid name clashes?
	auto low = Encode(*formula.low_value, tag);
	auto high = Encode(*formula.high_value, tag);
	return z3::forall(key, MakeImplication((low <= key) && (key <= high), EncodeFlow(node, key, true, tag)));
}

z3::expr Encoder::Encode(StepTag tag, const ObligationAxiom& formula) {
	// std::string varname = "OBL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name;
	// return EncodeVariable(Sort::BOOL, varname, tag) == MakeTrue();
	return EncodeObligation(formula.kind, EncodeVariable(formula.key->decl, tag), tag);
}

z3::expr Encoder::Encode(StepTag tag, const FulfillmentAxiom& formula) {
	// std::string varname = "FUL_" + plankton::to_string(formula.kind) + "_" + formula.key->decl.name + "_" + (formula.return_value ? "true" : "false");
	// return (EncodeVariable(Sort::BOOL, varname, tag) == MakeTrue());
	return EncodeFulfillment(formula.kind, EncodeVariable(formula.key->decl, tag), formula.return_value, tag);
}

z3::expr Encoder::Encode(StepTag tag, const KeysetContainsAxiom& formula) {
	auto node = Encode(*formula.node, tag);
	auto value = Encode(*formula.value, tag);
	return EncodeKeysetContains(node, value, tag);
}

z3::expr Encoder::Encode(StepTag tag, const DataStructureLogicallyContainsAxiom& formula) {
	auto node = EncodeVariable(Sort::PTR, "qv-ptr", tag); // TODO: does this avoid name clashes?
	auto key = Encode(*formula.value, tag);
	auto logicallyContains = EncodePredicate(*postConfig.logicallyContainsKey, node, key, tag);
	auto keysetContains = EncodeKeysetContains(node, key, tag);
	return z3::exists(node, keysetContains && logicallyContains);
}

z3::expr Encoder::Encode(StepTag tag, const NodeLogicallyContainsAxiom& formula) {
	auto node = Encode(*formula.node, tag);
	auto key = Encode(*formula.value, tag);
	return EncodePredicate(*postConfig.logicallyContainsKey, node, key, tag);
}

z3::expr Encoder::Encode(StepTag /*tag*/, const PastPredicate& /*formula*/) {
	throw EncodingError("Cannot encode 'PastPredicate'.");
}

z3::expr Encoder::Encode(StepTag /*tag*/, const FuturePredicate& /*formula*/) {
	throw EncodingError("Cannot encode 'FuturePredicate'.");
}

z3::expr Encoder::Encode(StepTag tag, const Annotation& formula) {
	return Encode(tag, *formula.now);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

z3::solver Encoder::MakeSolver(StepTag tag) {
	// TODO: clean up, what to use?

	// create solver
	auto solver = z3::solver(context);

	// const Type& nodeType = postConfig.flowDomain->GetNodeType();

	// // add rule for solving flow
	// // forall node,selector,successor,key:  node->{selector}=successor /\ key in flow(node) /\ key_flows_out(node, key) ==> key in flow(successor)
	// for (auto [fieldName, fieldType] : GetNodeTypePointerFields()) {
	// 	auto key = EncodeVariable(Sort::DATA, "qv-flow-" + fieldName + "-key", tag);
	// 	auto node = EncodeVariable(nodeType.sort, "qv-flow-" + fieldName + "-node", tag);
	// 	auto successor = EncodeVariable(fieldType->sort, "qv-flow-" + fieldName + "-successor", tag);
	// 	auto selector = std::make_pair(&nodeType, fieldName);

	// 	solver.add(z3::forall(node, key, successor, MakeImplication(
	// 		EncodeHeap(node, selector, successor, tag) &&
	// 		EncodeHasFlow(node, tag) &&
	// 		EncodePredicate(postConfig.flowDomain->GetOutFlowContains(fieldName), node, key, tag),
	// 		/* ==> */
	// 		EncodeFlow(successor, key, tag)
	// 	)));
	// }

	// // add rule for solving ownership
	// // forall node:  owned(node) ==> !hasFlow(node)
	// auto node = EncodeVariable(Sort::PTR, "qv-owner-ptr", tag);
	// solver.add(z3::forall(node, MakeImplication(
	// 	EncodeOwnership(node, tag), /* ==> */ !EncodeHasFlow(node, tag)
	// )));

	// // add rule for solving ownership
	// // forall node,other,selector:  owned(node) ==> other->{selector} != node
	// for (auto [fieldName, fieldType] : GetNodeTypePointerFields()) {
	// 	auto node = EncodeVariable(nodeType.sort, "qv-owner-node", tag);
	// 	auto other = EncodeVariable(fieldType->sort, "qv-owner-other", tag);
	// 	auto selector = std::make_pair(&nodeType, fieldName);

	// 	solver.add(z3::forall(node, other, MakeImplication(
	// 		EncodeOwnership(node, tag), /* ==> */ !EncodeHeap(other, selector, node, tag)
	// 	)));
	// }

	// done
	return solver;
}

std::pair<z3::solver, z3::expr_vector> Encoder::MakePostSolver(std::size_t /*footprintSize*/) {
	throw std::logic_error("not yet implemented: Encoder::MakePostSolver(std::size_t)");

//	// create solver
//	auto solver = MakeSolver();
//
//	// create pointers into footprint
//	z3::expr_vector footprint(context);
//	for (std::size_t index = 0; index < footprintSize; ++index) {
//		std::string name = "fp$ptr-" + std::to_string(index);
//		auto var = context.constant(name.c_str(), EncodeSort(Sort::PTR));
//		footprint.push_back(var);
//	}
//
//	// footprint pointers are all distinct
//	if (footprintSize != 0) {
//		// TODO important: needed? pairwise distinct or null?
//		solver.add(z3::distinct(footprint));
//	}
//
//	// everything in the heap except 'footprint' remains unchanged (selectors, flow)
//	// note: we do not care for stack variables, this must be done elsewhere
//	// note: we do not handle purity nor obligations/fulfillments, this must be done elsewhere
//	const Type& nodeType = postConfig.flowDomain->GetNodeType();
//	auto node = EncodeVariable(nodeType.sort, "qv-rule-ptr", NOW);
//	auto key = EncodeVariable(Sort::DATA, "qv-rule-key", NOW);
//
//	z3::expr_vector vec(context);
//	for (auto changed : footprint) {
//		vec.push_back(changed != node);
//	}
//	auto nodeNotInFootprint = MakeAnd(vec);
//
//	solver.add(z3::forall(node, MakeImplication(
//		nodeNotInFootprint,
//		/* ==> */
//		EncodeTransitionMaintainsHeap(node, nodeType) &&
//		EncodeTransitionMaintainsOwnership(node) &&
//		z3::forall(key, EncodeTransitionMaintainsFlow(node, key))
//	)));
//
//	assert(solver.check() == z3::sat);
//	return std::make_pair(solver, footprint);
}
