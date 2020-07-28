#pragma once
#ifndef PLANKTON_ENCODER
#define PLANKTON_ENCODER


#include <set>
#include <memory>
#include <string>
#include <ostream>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "plankton/chkimp.hpp"


namespace plankton {

	struct EncodedSymbol {
		virtual ~EncodedSymbol() = default;
		virtual bool operator==(const EncodedSymbol& other) const = 0;
		virtual std::ostream& operator<<(std::ostream& stream) const = 0;
	};

	struct EncodedTerm {
		virtual ~EncodedTerm() = default;
		virtual bool operator==(const EncodedTerm& other) const = 0;
		virtual std::ostream& operator<<(std::ostream& stream) const = 0;

		virtual std::shared_ptr<EncodedTerm> Negate() const = 0;
		virtual std::shared_ptr<EncodedTerm> And(std::shared_ptr<EncodedTerm> term) const = 0;
		virtual std::shared_ptr<EncodedTerm> Or(std::shared_ptr<EncodedTerm> term) const = 0;
		virtual std::shared_ptr<EncodedTerm> XOr(std::shared_ptr<EncodedTerm> term) const = 0;
		virtual std::shared_ptr<EncodedTerm> Implies(std::shared_ptr<EncodedTerm> term) const = 0;
		virtual std::shared_ptr<EncodedTerm> Iff(std::shared_ptr<EncodedTerm> term) const = 0;
		virtual std::shared_ptr<EncodedTerm> Equal(std::shared_ptr<EncodedTerm> term) const = 0;
		virtual std::shared_ptr<EncodedTerm> Distinct(std::shared_ptr<EncodedTerm> term) const = 0;
	};


	struct Symbol final {
		std::shared_ptr<EncodedSymbol> repr;
		Symbol(std::shared_ptr<EncodedSymbol> repr_) : repr(std::move(repr_)) { assert(repr); }

		inline bool operator==(const Symbol& other) const { return *repr == *other.repr; }
		inline std::ostream& operator<<(std::ostream& stream) const { return repr->operator<<(stream); }
	};

	struct Term final {
		std::shared_ptr<EncodedTerm> repr;
		Term(std::shared_ptr<EncodedTerm> repr_) : repr(std::move(repr_)) { assert(repr); }

		inline bool operator==(const Term& other) const { return *repr == *other.repr; }
		inline std::ostream& operator<<(std::ostream& stream) const { return repr->operator<<(stream); }

		inline Term Negate() const { return Term(repr->Negate()); }
		inline Term And(Term term) const { return Term(repr->And(term.repr)); }
		inline Term Or(Term term) const { return Term(repr->Or(term.repr)); }
		inline Term XOr(Term term) const { return Term(repr->XOr(term.repr)); }
		inline Term Implies(Term term) const { return Term(repr->Implies(term.repr)); }
		inline Term Iff(Term term) const { return Term(repr->Iff(term.repr)); }
		inline Term Equal(Term term) const { return Term(repr->Equal(term.repr)); }
		inline Term Distinct(Term term) const { return Term(repr->Distinct(term.repr)); }
	};

	struct Selector {
		const cola::Type& type;
		std::string fieldname;

		Selector(const cola::Type& type, std::string fieldname);
		bool operator==(const Selector& other) const;
		bool operator<(const Selector& other) const;
		std::ostream& operator<<(std::ostream& stream) const;
	};


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

		virtual Term MakeOr(const std::vector<Term>& disjuncts) = 0;
		virtual Term MakeAnd(const std::vector<Term>& conjuncts) = 0;
		virtual Term MakeExists(std::vector<Symbol> vars, Term term) = 0;
		virtual Term MakeForall(std::vector<Symbol> vars, Term term) = 0;
		// TODO: virtual std::vector<Symbol> MakeQuantifiedVariables(std::vector<std::reference_wrapper<const cola::Type>> types) = 0;
		
		virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker(EncodingTag tag = EncodingTag::NOW) = 0;

		virtual Symbol Encode(const cola::VariableDeclaration& decl, EncodingTag tag) = 0;
		virtual Term Encode(const heal::Formula& formula, EncodingTag tag) = 0;
		virtual Term Encode(const cola::Expression& expression, EncodingTag tag) = 0;
		virtual Term EncodeFlow(Term node, Term value, EncodingTag tag) = 0;
		virtual Term EncodeHeap(Term node, Selector selector, EncodingTag tag) = 0;
		virtual Term EncodeHeapIs(Term node, Selector selector, Term value, EncodingTag tag) = 0;
		virtual Term EncodeHasFlow(Term node, EncodingTag tag) = 0;
		virtual Term EncodeIsOwned(Term node, EncodingTag tag) = 0;
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

		template<typename T, typename... U>
		inline std::vector<T> MakeVector(U... elems) {
			return std::vector<T>(std::forward<U>(elems)...);
		}
		template<typename... T>
		inline Term MakeExists(T... vars, Term term) {
			return MakeExists(MakeVector<Symbol>(std::forward<T>(vars)...), std::move(term));
		}
		template<typename... T>
		inline Term MakeForall(T... vars, Term term) {
			return MakeForall(MakeVector<Symbol>(std::forward<T>(vars)...), std::move(term));
		}

	};

} // namespace plankton

#endif