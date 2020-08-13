#include "plankton/backend/z3encoder.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


Term Z3Encoder::MakeNullPtr() {
	return MakeZ3NullPtr();
}

Term Z3Encoder::MakeMinValue() {
	return MakeZ3MinValue();
}

Term Z3Encoder::MakeMaxValue() {
	return MakeZ3MaxValue();
}

Term Z3Encoder::MakeBool(bool value) {
	return MakeZ3Bool(value);
}

Term Z3Encoder::MakeDataBounds(Term term) {
	return MakeZ3DataBounds(Z3Expr(term));
}


template<typename T>
std::vector<Z3Expr> InternalizeVector(const std::vector<T>& vector) {
	std::vector<Z3Expr> result;
	result.reserve(vector.size());
	for (auto elem : vector) {
		result.emplace_back(std::move(elem));
	}
	return result;
}

Term Z3Encoder::MakeOr(const std::vector<Term>& disjuncts) {
	return MakeZ3Or(InternalizeVector(disjuncts));
}

Term Z3Encoder::MakeAnd(const std::vector<Term>& conjuncts) {
	return MakeZ3And(InternalizeVector(conjuncts));
}

Term Z3Encoder::MakeAtMostOne(const std::vector<Term>& elements) {
	return MakeZ3AtMostOne(InternalizeVector(elements));
}

Term Z3Encoder::MakeExists(const std::vector<Symbol>& vars, Term term) {
	return MakeZ3Exists(InternalizeVector(vars), Z3Expr(term));
}

Term Z3Encoder::MakeForall(const std::vector<Symbol>& vars, Term term) {
	return MakeZ3Forall(InternalizeVector(vars), Z3Expr(term));
}


Symbol Z3Encoder::Encode(const cola::VariableDeclaration& decl, EncodingTag tag) {
	return EncodeZ3(decl, tag);
}

Term Z3Encoder::Encode(const heal::Formula& formula, EncodingTag tag) {
	return EncodeZ3(formula, tag);
}

Term Z3Encoder::Encode(const cola::Expression& expression, EncodingTag tag) {
	return EncodeZ3(expression, tag);
}

Term Z3Encoder::EncodeFlow(Term node, Term value, EncodingTag tag) {
	return EncodeZ3Flow(Z3Expr(node), Z3Expr(value), tag);
}

Term Z3Encoder::EncodeHeap(Term node, Selector selector, EncodingTag tag) {
	return EncodeZ3Heap(Z3Expr(node), selector, tag);
}

Term Z3Encoder::EncodeHeapIs(Term node, Selector selector, Term value, EncodingTag tag) {
	return EncodeZ3HeapIs(Z3Expr(node), selector, Z3Expr(value), tag);
}

Term Z3Encoder::EncodeHasFlow(Term node, EncodingTag tag) {
	return EncodeZ3HasFlow(Z3Expr(node), tag);
}

Term Z3Encoder::EncodeIsOwned(Term node, EncodingTag tag) {
	return EncodeZ3IsOwned(Z3Expr(node), tag);
}

Term Z3Encoder::EncodeUniqueInflow(Term node, Term value, EncodingTag tag) {
	return EncodeZ3UniqueInflow(Z3Expr(node), Z3Expr(value), tag);
}

Term Z3Encoder::EncodeKeysetContains(Term node, Term value, EncodingTag tag) {
	return EncodeZ3KeysetContains(Z3Expr(node), Z3Expr(value), tag);
}

Term Z3Encoder::EncodeObligation(heal::SpecificationAxiom::Kind kind, Term value, EncodingTag tag) {
	return EncodeZ3Obligation(kind, Z3Expr(value), tag);
}

Term Z3Encoder::EncodeFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue, EncodingTag tag) {
	return EncodeZ3Fulfillment(kind, Z3Expr(value), returnValue, tag);
}

Term Z3Encoder::EncodeInvariant(const heal::Invariant& invariant, Term arg, EncodingTag tag) {
	return EncodeZ3Invariant(invariant, Z3Expr(arg), tag);
}

Term Z3Encoder::EncodePredicate(const heal::Predicate& predicate, Term arg1, Term arg2, EncodingTag tag) {
	return EncodeZ3Predicate(predicate, Z3Expr(arg1), Z3Expr(arg2), tag);
}


Term Z3Encoder::EncodeTransitionMaintainsOwnership(Term node) {
	return EncodeZ3TransitionMaintainsOwnership(Z3Expr(node));
}

Term Z3Encoder::EncodeTransitionMaintainsFlow(Term node, Term key) {
	return EncodeZ3TransitionMaintainsFlow(Z3Expr(node), Z3Expr(key));
}

Term Z3Encoder::EncodeTransitionMaintainsHeap(Term node, Selector selector) {
	return EncodeZ3TransitionMaintainsHeap(Z3Expr(node), selector);
}
