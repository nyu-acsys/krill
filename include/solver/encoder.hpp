#pragma once
#ifndef SOLVER_ENCODER
#define SOLVER_ENCODER


#include <set>
#include <memory>
#include <string>
#include <ostream>
#include <utility>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/properties.hpp"
#include "config.hpp"
#include "encoding.hpp"
#include "chkimp.hpp"


namespace solver {

	enum struct EncodingTag { NOW, NEXT };

	struct Encoder {
		explicit Encoder(std::shared_ptr<SolverConfig> config) : config(std::move(config)) {}
		virtual ~Encoder() = default;

		[[nodiscard]] const SolverConfig& Config() const { return *config; }
        [[nodiscard]] virtual std::unique_ptr<ImplicationChecker> MakeImplicationChecker(EncodingTag tag) = 0;
        [[nodiscard]] std::unique_ptr<ImplicationChecker> MakeImplicationChecker() { return MakeImplicationChecker(EncodingTag::NOW); }
        [[nodiscard]] std::unique_ptr<ImplicationChecker> MakeImplicationCheckerNow() { return MakeImplicationChecker(EncodingTag::NOW); }
        [[nodiscard]] std::unique_ptr<ImplicationChecker> MakeImplicationCheckerNext() { return MakeImplicationChecker(EncodingTag::NEXT); }

        [[nodiscard]] virtual Term MakeNullPtr() = 0;
        [[nodiscard]] virtual Term MakeMinValue() = 0;
        [[nodiscard]] virtual Term MakeMaxValue() = 0;
        [[nodiscard]] virtual Term MakeBool(bool value) = 0;
        [[nodiscard]] inline Term MakeTrue() { return MakeBool(true); }
        [[nodiscard]] inline Term MakeFalse() { return MakeBool(false); }

        [[nodiscard]] static inline Term MakeNegation(const Term& term) { return term.Negate(); }
        [[nodiscard]] static inline Term MakeAnd(const Term& term, const Term& other) { return term.And(other); }
        [[nodiscard]] static inline Term MakeOr(const Term& term, const Term& other) { return term.Or(other); }
        [[nodiscard]] static inline Term MakeXOr(const Term& term, const Term& other) { return term.XOr(other); }
        [[nodiscard]] static inline Term MakeImplies(const Term& term, const Term& other) { return term.Implies(other); }
        [[nodiscard]] static inline Term MakeIff(const Term& term, const Term& other) { return term.Iff(other); }
        [[nodiscard]] static inline Term MakeEqual(const Term& term, const Term& other) { return term.Equal(other); }
        [[nodiscard]] static inline Term MakeDistinct(const Term& term, const Term& other) { return term.Distinct(other); }

        [[nodiscard]] virtual Term MakeOr(const std::vector<Term>& disjuncts) = 0;
        [[nodiscard]] virtual Term MakeAnd(const std::vector<Term>& conjuncts) = 0;
        [[nodiscard]] virtual Term MakeAtMostOne(const std::vector<Term>& elements) = 0;
        [[nodiscard]] virtual Term MakeExists(const std::vector<Symbol>& vars, Term term) = 0;
        [[nodiscard]] virtual Term MakeForall(const std::vector<Symbol>& vars, Term term) = 0;
        [[nodiscard]] virtual Term MakeDataBounds(Term term) = 0; // MakeMinValue() <= term <= MakeMaxValue()

		[[nodiscard]] virtual Symbol Encode(const cola::VariableDeclaration& decl, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term Encode(const heal::Formula& formula, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term Encode(const cola::Expression& expression, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeFlow(Term node, Term value, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeHeap(Term node, Selector selector, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeHeapIs(Term node, Selector selector, Term value, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeHasFlow(Term node, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeIsOwned(Term node, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeUniqueInflow(Term node, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeKeysetContains(Term node, Term value, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeObligation(heal::SpecificationAxiom::Kind kind, Term value, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodeInvariant(const heal::Invariant& invariant, Term arg, EncodingTag tag) = 0;
		[[nodiscard]] virtual Term EncodePredicate(const heal::Predicate& predicate, Term arg1, Term arg2, EncodingTag tag) = 0;

		[[nodiscard]] inline Symbol EncodeNow(const cola::VariableDeclaration& decl) { return Encode(decl, EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNow(const heal::Formula& formula) { return Encode(formula, EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNow(const cola::Expression& expression) { return Encode(expression, EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowFlow(Term node, Term value) { return EncodeFlow(std::move(node), std::move(value), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowHeap(Term node, Selector selector) { return EncodeHeap(std::move(node), std::move(selector), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowHeapIs(Term node, Selector selector, Term value) { return EncodeHeapIs(std::move(node), std::move(selector), std::move(value), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowHasFlow(Term node) { return EncodeHasFlow(std::move(node), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowIsOwned(Term node) { return EncodeIsOwned(std::move(node), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowUniqueInflow(Term node) { return EncodeUniqueInflow(std::move(node), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowKeysetContains(Term node, Term value) { return EncodeKeysetContains(std::move(node), std::move(value), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowObligation(heal::SpecificationAxiom::Kind kind, Term value) { return EncodeObligation(kind, std::move(value), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue) { return EncodeFulfillment(kind, std::move(value), returnValue, EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowInvariant(const heal::Invariant& invariant, Term arg) { return EncodeInvariant(invariant, std::move(arg), EncodingTag::NOW); }
		[[nodiscard]] inline Term EncodeNowPredicate(const heal::Predicate& predicate, Term arg1, Term arg2) { return EncodePredicate(predicate, std::move(arg1), std::move(arg2), EncodingTag::NOW); }

		[[nodiscard]] inline Symbol EncodeNext(const cola::VariableDeclaration& decl) { return Encode(decl, EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNext(const heal::Formula& formula) { return Encode(formula, EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNext(const cola::Expression& expression) { return Encode(expression, EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextFlow(Term node, Term value) { return EncodeFlow(std::move(node), std::move(value), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextHeap(Term node, Selector selector) { return EncodeHeap(std::move(node), std::move(selector), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextHeapIs(Term node, Selector selector, Term value) { return EncodeHeapIs(std::move(node), std::move(selector), std::move(value), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextHasFlow(Term node) { return EncodeHasFlow(std::move(node), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextIsOwned(Term node) { return EncodeIsOwned(std::move(node), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextUniqueInflow(Term node) { return EncodeUniqueInflow(std::move(node), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextKeysetContains(Term node, Term value) { return EncodeKeysetContains(std::move(node), std::move(value), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextObligation(heal::SpecificationAxiom::Kind kind, Term value) { return EncodeObligation(kind, std::move(value), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue) { return EncodeFulfillment(kind, std::move(value), returnValue, EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextInvariant(const heal::Invariant& invariant, Term arg) { return EncodeInvariant(invariant, std::move(arg), EncodingTag::NEXT); }
		[[nodiscard]] inline Term EncodeNextPredicate(const heal::Predicate& predicate, Term arg1, Term arg2) { return EncodePredicate(predicate, std::move(arg1), std::move(arg2), EncodingTag::NEXT); }

        [[nodiscard]] virtual Term EncodeTransitionMaintainsOwnership(Term node) = 0;
        [[nodiscard]] virtual Term EncodeTransitionMaintainsFlow(Term node, Term key) = 0;
        [[nodiscard]] virtual Term EncodeTransitionMaintainsHeap(Term node, Selector selector) = 0;
        [[nodiscard]] inline Term EncodeTransitionMaintainsHeap(Term node, const cola::Type& nodeType, std::set<std::string> excludedSelectors = {}) {
			std::vector<Term> terms;
			terms.reserve(nodeType.fields.size());
			for (auto [fieldName, fieldType] : nodeType.fields) {
				if (excludedSelectors.count(fieldName) != 0) continue;
				terms.push_back(EncodeTransitionMaintainsHeap(node, Selector(nodeType, fieldName)));
			}
			return MakeAnd(terms);
		}

		template<typename... T>
        [[nodiscard]] inline std::pair<std::vector<Symbol>, Term> Extract(Term term) {
			return { {}, term };
		}
		template<typename... T>
        [[nodiscard]] inline std::pair<std::vector<Symbol>, Term> Extract(Symbol symbol, T... args) {
			auto result = Extract(std::forward<T>(args)...);
			result.first.insert(result.first.begin(), symbol);
			return result;
		}
		template<typename... T>
        [[nodiscard]] inline Term MakeExists(T... args) {
			auto [vector, term] = Extract(std::forward<T>(args)...);
			return MakeExists(std::move(vector), std::move(term));
		}
		template<typename... T>
        [[nodiscard]] inline Term MakeForall(T... args) {
			auto [vector, term] = Extract(std::forward<T>(args)...);
			return MakeForall(std::move(vector), std::move(term));
		}

        private:
            std::shared_ptr<SolverConfig> config;
	};

    [[nodiscard]] std::unique_ptr<Encoder> MakeDefaultEncoder(std::shared_ptr<SolverConfig> config);

} // namespace solver


#endif