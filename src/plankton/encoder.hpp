#pragma once
#ifndef PLANKTON_ENCODER
#define PLANKTON_ENCODER


#include <set>
#include <memory>
#include <string>
#include <ostream>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "plankton/solver.hpp"
#include "plankton/encoding.hpp"
#include "plankton/chkimp.hpp"


namespace plankton {

	enum struct EncodingTag { NOW, NEXT };

	struct Encoder {
		const PostConfig& config;
		Encoder(const PostConfig& config) : config(config) {}
		virtual ~Encoder() = default;

		virtual Term MakeNullPtr() = 0;
		virtual Term MakeMinValue() = 0;
		virtual Term MakeMaxValue() = 0;
		virtual Term MakeBool(bool value) = 0;
		inline Term MakeTrue() { return MakeBool(true); }
		inline Term MakeFalse() { return MakeBool(false); }

		inline Term MakeNegation(Term term) const { return term.Negate(); }
		inline Term MakeAnd(Term term, Term other) const { return term.And(other); }
		inline Term MakeOr(Term term, Term other) const { return term.Or(other); }
		inline Term MakeXOr(Term term, Term other) const { return term.XOr(other); }
		inline Term MakeImplies(Term term, Term other) const { return term.Implies(other); }
		inline Term MakeIff(Term term, Term other) const { return term.Iff(other); }
		inline Term MakeEqual(Term term, Term other) const { return term.Equal(other); }
		inline Term MakeDistinct(Term term, Term other) const { return term.Distinct(other); }

		virtual Term MakeOr(const std::vector<Term>& disjuncts) = 0;
		virtual Term MakeAnd(const std::vector<Term>& conjuncts) = 0;
		virtual Term MakeAtMostOne(const std::vector<Term>& elements) = 0;
		virtual Term MakeExists(const std::vector<Symbol>& vars, Term term) = 0;
		virtual Term MakeForall(const std::vector<Symbol>& vars, Term term) = 0;

		virtual Term MakeDataBounds(Term term) = 0; // MakeMinValue() <= term <= MakeMaxValue()		
		virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker(EncodingTag tag = EncodingTag::NOW) = 0;

		virtual Symbol Encode(const cola::VariableDeclaration& decl, EncodingTag tag) = 0;
		virtual Term Encode(const heal::Formula& formula, EncodingTag tag) = 0;
		virtual Term Encode(const cola::Expression& expression, EncodingTag tag) = 0;
		virtual Term EncodeFlow(Term node, Term value, EncodingTag tag) = 0;
		virtual Term EncodeHeap(Term node, Selector selector, EncodingTag tag) = 0;
		virtual Term EncodeHeapIs(Term node, Selector selector, Term value, EncodingTag tag) = 0;
		virtual Term EncodeHasFlow(Term node, EncodingTag tag) = 0;
		virtual Term EncodeIsOwned(Term node, EncodingTag tag) = 0;
		virtual Term EncodeUniqueInflow(Term node, Term value, EncodingTag tag) = 0;
		virtual Term EncodeKeysetContains(Term node, Term value, EncodingTag tag) = 0;
		virtual Term EncodeObligation(heal::SpecificationAxiom::Kind kind, Term value, EncodingTag tag) = 0;
		virtual Term EncodeFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue, EncodingTag tag) = 0;
		virtual Term EncodeInvariant(const heal::Invariant& invariant, Term arg, EncodingTag tag) = 0;
		virtual Term EncodePredicate(const heal::Predicate& predicate, Term arg1, Term arg2, EncodingTag tag) = 0;

		inline Symbol EncodeNow(const cola::VariableDeclaration& decl) { return Encode(decl, EncodingTag::NOW); }
		inline Term EncodeNow(const heal::Formula& formula) { return Encode(formula, EncodingTag::NOW); }
		inline Term EncodeNow(const cola::Expression& expression) { return Encode(expression, EncodingTag::NOW); }
		inline Term EncodeNowFlow(Term node, Term value) { return EncodeFlow(node, value, EncodingTag::NOW); }
		inline Term EncodeNowHeap(Term node, Selector selector) { return EncodeHeap(node, selector, EncodingTag::NOW); }
		inline Term EncodeNowHeapIs(Term node, Selector selector, Term value) { return EncodeHeapIs(node, selector, value, EncodingTag::NOW); }
		inline Term EncodeNowHasFlow(Term node) { return EncodeHasFlow(node, EncodingTag::NOW); }
		inline Term EncodeNowIsOwned(Term node) { return EncodeIsOwned(node, EncodingTag::NOW); }
		inline Term EncodeNowUniqueInflow(Term node, Term value) { return EncodeUniqueInflow(node, value, EncodingTag::NOW); }
		inline Term EncodeNowKeysetContains(Term node, Term value) { return EncodeKeysetContains(node, value, EncodingTag::NOW); }
		inline Term EncodeNowObligation(heal::SpecificationAxiom::Kind kind, Term value) { return EncodeObligation(kind, value, EncodingTag::NOW); }
		inline Term EncodeNowFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue) { return EncodeFulfillment(kind, value, returnValue, EncodingTag::NOW); }
		inline Term EncodeNowInvariant(const heal::Invariant& invariant, Term arg) { return EncodeInvariant(invariant, arg, EncodingTag::NOW); }
		inline Term EncodeNowPredicate(const heal::Predicate& predicate, Term arg1, Term arg2) { return EncodePredicate(predicate, arg1, arg2, EncodingTag::NOW); }

		inline Symbol EncodeNext(const cola::VariableDeclaration& decl) { return Encode(decl, EncodingTag::NEXT); }
		inline Term EncodeNext(const heal::Formula& formula) { return Encode(formula, EncodingTag::NEXT); }
		inline Term EncodeNext(const cola::Expression& expression) { return Encode(expression, EncodingTag::NEXT); }
		inline Term EncodeNextFlow(Term node, Term value) { return EncodeFlow(node, value, EncodingTag::NEXT); }
		inline Term EncodeNextHeap(Term node, Selector selector) { return EncodeHeap(node, selector, EncodingTag::NEXT); }
		inline Term EncodeNextHeapIs(Term node, Selector selector, Term value) { return EncodeHeapIs(node, selector, value, EncodingTag::NEXT); }
		inline Term EncodeNextHasFlow(Term node) { return EncodeHasFlow(node, EncodingTag::NEXT); }
		inline Term EncodeNextIsOwned(Term node) { return EncodeIsOwned(node, EncodingTag::NEXT); }
		inline Term EncodeNextUniqueInflow(Term node, Term value) { return EncodeUniqueInflow(node, value, EncodingTag::NEXT); }
		inline Term EncodeNextKeysetContains(Term node, Term value) { return EncodeKeysetContains(node, value, EncodingTag::NEXT); }
		inline Term EncodeNextObligation(heal::SpecificationAxiom::Kind kind, Term value) { return EncodeObligation(kind, value, EncodingTag::NEXT); }
		inline Term EncodeNextFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue) { return EncodeFulfillment(kind, value, returnValue, EncodingTag::NEXT); }
		inline Term EncodeNextInvariant(const heal::Invariant& invariant, Term arg) { return EncodeInvariant(invariant, arg, EncodingTag::NEXT); }
		inline Term EncodeNextPredicate(const heal::Predicate& predicate, Term arg1, Term arg2) { return EncodePredicate(predicate, arg1, arg2, EncodingTag::NEXT); }

		virtual Term EncodeTransitionMaintainsOwnership(Term node) = 0;
		virtual Term EncodeTransitionMaintainsFlow(Term node, Term key) = 0;
		virtual Term EncodeTransitionMaintainsHeap(Term node, Selector selector) = 0;
		inline Term EncodeTransitionMaintainsHeap(Term node, const cola::Type& nodeType, std::set<std::string> excludedSelectors = {}) {
			std::vector<Term> terms;
			terms.reserve(nodeType.fields.size());
			for (auto [fieldName, fieldType] : nodeType.fields) {
				if (excludedSelectors.count(fieldName) != 0) continue;
				terms.push_back(EncodeTransitionMaintainsHeap(node, Selector(nodeType, fieldName)));
			}
			return MakeAnd(std::move(terms));
		}

		template<typename... T>
		inline std::pair<std::vector<Symbol>, Term> Extract(Term term) {
			return { {}, term };
		}
		template<typename... T>
		inline std::pair<std::vector<Symbol>, Term> Extract(Symbol symbol, T... args) {
			auto result = Extract(std::forward<T>(args)...);
			result.first.insert(result.first.begin(), symbol);
			return result;
		}
		template<typename... T>
		inline Term MakeExists(T... args) {
			auto [vector, term] = Extract(std::forward<T>(args)...);
			return MakeExists(std::move(vector), std::move(term));
		}
		template<typename... T>
		inline Term MakeForall(T... args) {
			auto [vector, term] = Extract(std::forward<T>(args)...);
			return MakeForall(std::move(vector), std::move(term));
		}

	};

} // namespace plankton


#endif