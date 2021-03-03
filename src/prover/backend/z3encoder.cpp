#include "prover/backend/z3encoder.hpp"

#include "prover/backend/z3chkimp.hpp"

using namespace cola;
using namespace heal;
using namespace prover;


std::vector<std::pair<std::string, const Type*>> GetPointerFields(const PostConfig& config) { // TODO: needed?
	std::vector<std::pair<std::string, const Type*>> result;
	for (auto [fieldName, fieldType] : config.flowDomain->GetNodeType().fields) {
		if (fieldType.get().sort != Sort::PTR) continue;
		result.push_back(std::make_pair(fieldName, &fieldType.get()));
	}
	return result;
}

inline z3::sort MakePointerSort(z3::context& context) {
	return context.int_sort(); // TODO important: use proper sort (with minimal overhead)
}

inline z3::sort MakeDataSort(z3::context& context) {
	return context.int_sort(); // TODO important: use proper sort (with minimal overhead)
}

inline z3::sort MakeValueSort(z3::context& context) {
	return context.int_sort(); // TODO important: use proper sort (with minimal overhead)
}

inline z3::sort MakeBoolSort(z3::context& context) {
	return context.bool_sort();
}

inline z3::sort MakeSelectorSort(z3::context& context) {
	return context.int_sort(); // TODO important: use proper sort (with minimal overhead)
}

inline z3::sort MakeSpecificationSort(z3::context& context) {
	// return context.uninterpreted_sort("specSort");
	return context.int_sort(); // TODO important: use proper sort (enum sort)
}

Z3Encoder::Z3Encoder(const PostConfig& config) :
	Encoder(config),
	ptrSort(MakePointerSort(context)),
	dataSort(MakeDataSort(context)),
	valueSort(MakeValueSort(context)),
	boolSort(MakeBoolSort(context)),
	selectorSort(MakeSelectorSort(context)),
	specSort(MakeSpecificationSort(context)),
	nullPtr(context.constant("_NULL_", ptrSort)), 
	minVal(context.constant("_MIN_", dataSort)),
	maxVal(context.constant("_MAX_", dataSort)),
	heapNow(context.function("$MEM-now", ptrSort, selectorSort, valueSort)),
	heapNext(context.function("$MEM-next", ptrSort, selectorSort, valueSort)),
	flowNow(context.function("$FLOW-now", ptrSort, dataSort, boolSort)),
	flowNext(context.function("$FLOW-next", ptrSort, dataSort, boolSort)),
	uniqueInflowNow(context.function("$uINFLOW-now", ptrSort, boolSort)),
	uniqueInflowNext(context.function("$uINFLOW-next", ptrSort, boolSort)),
	ownershipNow(context.function("$OWN-now", ptrSort, boolSort)),
	ownershipNext(context.function("$OWN-next", ptrSort, boolSort)),
	obligationNow(context.function("$OBL-now", dataSort, specSort, boolSort)),
	obligationNext(context.function("$OBL-next", dataSort, specSort, boolSort)),
	fulfillmentNow(context.function("$FUL-now", dataSort, specSort, boolSort, boolSort)),
	fulfillmentNext(context.function("$FUL-next", dataSort, specSort, boolSort, boolSort)),
	pointerFields(GetPointerFields(config))
{
	// TODO: currently nullPtr/minVal/maxValu are just some symbols that are not bound to a value
}

template<typename M, typename K, typename F>
inline z3::expr GetOrCreate(M& map, const K& key, F MakeNew) {
	auto find = map.find(key);
	if (find != map.end()) {
		return find->second;
	}
	auto insert = map.insert(std::make_pair(key, MakeNew()));
	assert(insert.second);
	return insert.first->second;
}

inline std::string MakeNamePrefix(EncodingTag tag) {
	switch (tag) {
		case EncodingTag::NOW: return "var-now$";
		case EncodingTag::NEXT: return "var-next$";
	}
}

inline std::string MakeName(std::string name, EncodingTag tag) {
	return MakeNamePrefix(tag) + std::move(name);
}

inline std::string MakeName(const VariableDeclaration& decl, EncodingTag tag) {
	return MakeName(decl.name, tag);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

Z3Expr Z3Encoder::MakeZ3NullPtr() {
	return nullPtr;
}

Z3Expr Z3Encoder::MakeZ3MinValue() {
	return minVal;
}

Z3Expr Z3Encoder::MakeZ3MaxValue() {
	return maxVal;
}

Z3Expr Z3Encoder::MakeZ3Bool(bool value) {
	return context.bool_val(value);
}

Z3Expr Z3Encoder::MakeZ3DataBounds(Z3Expr value) {
	return (MakeZ3MinValue().expr <= value.expr) && (value.expr <= MakeZ3MaxValue().expr);
}


z3::expr_vector Z3Encoder::MakeZ3ExprVector() {
	return z3::expr_vector(context);
}

z3::expr_vector Z3Encoder::MakeZ3ExprVector(const std::vector<Z3Expr>& vector) {
	z3::expr_vector result(context);
	for (z3::expr expr : vector) {
		result.push_back(std::move(expr));
	}
	return result;
}

Z3Expr Z3Encoder::MakeZ3Or(const std::vector<Z3Expr>& disjuncts) {
	return z3::mk_or(MakeZ3ExprVector(disjuncts));
}

Z3Expr Z3Encoder::MakeZ3And(const std::vector<Z3Expr>& conjuncts) {
	return z3::mk_and(MakeZ3ExprVector(conjuncts));
}

Z3Expr Z3Encoder::MakeZ3AtMostOne(const std::vector<Z3Expr>& elements) {
	switch (elements.size()) {
		case 0: return MakeZ3True();
		case 1: return elements.at(0);
		default: return z3::atmost(MakeZ3ExprVector(elements), 1);
	}
}

Z3Expr Z3Encoder::MakeZ3Exists(const std::vector<Z3Expr>& vars, Z3Expr term) {
	return z3::exists(MakeZ3ExprVector(vars), term);
}

Z3Expr Z3Encoder::MakeZ3Forall(const std::vector<Z3Expr>& vars, Z3Expr term) {
	return z3::forall(MakeZ3ExprVector(vars), term);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

z3::sort Z3Encoder::EncodeZ3Sort(Sort sort) {
	switch (sort) {
		case Sort::PTR: return ptrSort;
		case Sort::DATA: return dataSort;
		case Sort::BOOL: return boolSort;
		case Sort::VOID: throw Z3EncodingError("Cannot represent cola::Sort::VOID as z3::sort.");
	}
}

Z3Expr Z3Encoder::EncodeZ3(const VariableDeclaration& decl, EncodingTag tag) {
	auto key = std::make_pair(&decl, tag);
	return GetOrCreate(decl2expr, key, [this,tag,&decl](){
		return context.constant(MakeName(decl, tag).c_str(), EncodeZ3Sort(decl.type.sort));
	});
}

Z3Expr Z3Encoder::EncodeZ3Variable(Sort sort, std::string name, EncodingTag tag) {
	// TODO: this may reuse an existing variable with the same name but a different sort
	auto key = std::make_pair(name, tag);
	return GetOrCreate(name2expr, key, [this,tag,&name,&sort](){
		return context.constant(MakeName(name, tag).c_str(), EncodeZ3Sort(sort));
	});
}

Z3Expr Z3Encoder::EncodeZ3Selector(Selector selector, EncodingTag /*tag*/) {
	if (!selector.type.has_field(selector.fieldname)) {
		throw Z3EncodingError("Cannot encode selector: type '" + selector.type.name + "' has no field '" + selector.fieldname + "'.");
	}
	return GetOrCreate(selector2expr, selector, [this](){
		return context.int_val(selector_count++);
	});
}


Z3Expr Z3Encoder::EncodeZ3Flow(Z3Expr node, Z3Expr value, EncodingTag tag) {
	switch (tag) {
		case EncodingTag::NOW: return flowNow(node, value) == MakeZ3True();
		case EncodingTag::NEXT: return flowNext(node, value) == MakeZ3True();
	}
}

Z3Expr Z3Encoder::EncodeZ3Heap(Z3Expr node, Selector selector, EncodingTag tag) {
	switch (tag) {
		case EncodingTag::NOW: return heapNow(node, EncodeZ3Selector(selector, tag));
		case EncodingTag::NEXT: return heapNext(node, EncodeZ3Selector(selector, tag));
	}
}

Z3Expr Z3Encoder::EncodeZ3HeapIs(Z3Expr node, Selector selector, Z3Expr value, EncodingTag tag) {
	return EncodeZ3Heap(node, selector, tag).Equal(value);
}

Z3Expr Z3Encoder::EncodeZ3HasFlow(Z3Expr node, EncodingTag tag) {
	auto key = EncodeZ3Variable(Sort::DATA, "qv-key", tag);
	return MakeZ3Exists({ key }, EncodeZ3Flow(node, key, tag));
}

Z3Expr Z3Encoder::EncodeZ3IsOwned(Z3Expr node, EncodingTag tag) {
	switch (tag) {
		case EncodingTag::NOW: return ownershipNow(node) == MakeZ3True();
		case EncodingTag::NEXT: return ownershipNext(node) == MakeZ3True();
	}
}

Z3Expr Z3Encoder::EncodeZ3UniqueInflow(Z3Expr node, EncodingTag tag) {
	switch (tag) {
		case EncodingTag::NOW: return uniqueInflowNow(node) == MakeZ3True();
		case EncodingTag::NEXT: return uniqueInflowNext(node) == MakeZ3True();
	}
}

Z3Expr Z3Encoder::EncodeZ3KeysetContains(Z3Expr node, Z3Expr value, EncodingTag tag) {
	z3::expr_vector vec(context);
	for (auto [fieldName, fieldType] : pointerFields) {
		vec.push_back(EncodeZ3Predicate(config.flowDomain->GetOutFlowContains(fieldName), node, value, tag));
	}
	z3::expr keyInOutflow = z3::mk_or(vec);
	return EncodeZ3Flow(node, value, tag) && !keyInOutflow;
}

Z3Expr Z3Encoder::EncodeZ3Spec(SpecificationAxiom::Kind kind) {
	switch (kind) {
		case SpecificationAxiom::Kind::CONTAINS: return context.int_val(0);
		case SpecificationAxiom::Kind::INSERT: return context.int_val(1);
		case SpecificationAxiom::Kind::DELETE: return context.int_val(2);
	}
}

Z3Expr Z3Encoder::EncodeZ3Obligation(SpecificationAxiom::Kind kind, Z3Expr value, EncodingTag tag) {
	switch (tag) {
		case EncodingTag::NOW: return obligationNow(value, EncodeZ3Spec(kind)) == MakeZ3True();
		case EncodingTag::NEXT: return obligationNext(value, EncodeZ3Spec(kind)) == MakeZ3True();
	}
}

Z3Expr Z3Encoder::EncodeZ3Fulfillment(SpecificationAxiom::Kind kind, Z3Expr value, bool returnValue, EncodingTag tag) {
	switch (tag) {
		case EncodingTag::NOW: return fulfillmentNow(value, EncodeZ3Spec(kind), MakeZ3Bool(returnValue)) == MakeZ3True();
		case EncodingTag::NEXT: return fulfillmentNext(value, EncodeZ3Spec(kind), MakeZ3Bool(returnValue)) == MakeZ3True();
	}
}

template<std::size_t N, typename T>
inline Z3Expr EncodeProperty(Z3Encoder& encoder, z3::context& context, const Property<N, T>& property, std::array<Z3Expr, N> args, EncodingTag tag) {
	// we cannot instantiate 'property' with 'args' directly (works only for 'VariableDeclaration's)
	auto blueprint = encoder.EncodeZ3(*property.blueprint, tag);

	// get blueprint vars
	z3::expr_vector blueprint_vars(context);
	for (const auto& decl : property.vars) {
		blueprint_vars.push_back(encoder.EncodeZ3(*decl, tag));
	}

	// get actual values
	z3::expr_vector replacement(context);
	for (auto arg : args) {
		replacement.push_back(arg);
	}

	// instantiate with desired values
	assert(blueprint_vars.size() == replacement.size());
	return blueprint.expr.substitute(blueprint_vars, replacement);
}

Z3Expr Z3Encoder::EncodeZ3Invariant(const Invariant& invariant, Z3Expr arg, EncodingTag tag) {
	return EncodeProperty(*this, context, invariant, { arg }, tag);
}

Z3Expr Z3Encoder::EncodeZ3Predicate(const Predicate& predicate, Z3Expr arg1, Z3Expr arg2, EncodingTag tag) {
	return EncodeProperty(*this, context, predicate, { arg1, arg2 }, tag);
}


Z3Expr Z3Encoder::EncodeZ3TransitionMaintainsOwnership(Z3Expr node) {
	auto pre = EncodeZ3IsOwned(node, EncodingTag::NOW);
	auto post = EncodeZ3IsOwned(node, EncodingTag::NEXT);
	return pre.Iff(post);
}

Z3Expr Z3Encoder::EncodeZ3TransitionMaintainsFlow(Z3Expr node, Z3Expr key) {
	auto pre = EncodeZ3Flow(node, key, EncodingTag::NOW);
	auto post = EncodeZ3Flow(node, key, EncodingTag::NEXT);
	return pre.Iff(post);
}

Z3Expr Z3Encoder::EncodeZ3TransitionMaintainsHeap(Z3Expr node, Selector selector) {
	auto pre = EncodeZ3Heap(node, selector, EncodingTag::NOW);
	auto post = EncodeZ3Heap(node, selector, EncodingTag::NEXT);
	return pre.Equal(post);
}


std::unique_ptr<ImplicationChecker> Z3Encoder::MakeImplicationChecker(EncodingTag tag) {
	z3::solver solver(context);
	// solver.set("mbqi", false);
	return std::make_unique<Z3ImplicationChecker>(shared_from_this(), tag, std::move(solver));
}
