#pragma once
#ifndef SOLVER_ENCODER_Z3ENCODER
#define SOLVER_ENCODER_Z3ENCODER


#include <memory>
#include "z3++.h"
#include "solver/config.hpp"
#include "solver/encoder.hpp"
#include "z3error.hpp"


namespace solver {

	struct Z3Expr final : public EncodedSymbol, public EncodedTerm {
		z3::expr expr;

		Z3Expr(z3::expr expr);
		Z3Expr(Term term);
		Z3Expr(Symbol symbol);
		Z3Expr(const EncodedTerm& term);
		Z3Expr(const EncodedSymbol& symbol);

		inline operator z3::expr() const { return expr; }
		inline operator Symbol() const { return Symbol(std::make_shared<Z3Expr>(*this)); }
		inline operator Term() const { return Term(std::make_shared<Z3Expr>(*this)); }
		[[nodiscard]] inline std::shared_ptr<EncodedTerm> ToTerm() const override { return std::make_shared<Z3Expr>(*this); }
		bool operator==(const EncodedTerm& other) const override;
		bool operator==(const EncodedSymbol& other) const override;
		bool operator==(const Z3Expr& other) const;
		
		void Print(std::ostream& stream) const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> Negate() const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> And(std::shared_ptr<EncodedTerm> term) const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> Or(std::shared_ptr<EncodedTerm> term) const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> XOr(std::shared_ptr<EncodedTerm> term) const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> Implies(std::shared_ptr<EncodedTerm> term) const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> Iff(std::shared_ptr<EncodedTerm> term) const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> Equal(std::shared_ptr<EncodedTerm> term) const override;
		[[nodiscard]] std::shared_ptr<EncodedTerm> Distinct(std::shared_ptr<EncodedTerm> term) const override;

		[[nodiscard]] Z3Expr Neg() const;
		[[nodiscard]] Z3Expr And(Z3Expr term) const;
		[[nodiscard]] Z3Expr Or(Z3Expr term) const;
		[[nodiscard]] Z3Expr XOr(Z3Expr term) const;
		[[nodiscard]] Z3Expr Implies(Z3Expr term) const;
		[[nodiscard]] Z3Expr Iff(Z3Expr term) const;
		[[nodiscard]] Z3Expr Equal(Z3Expr term) const;
		[[nodiscard]] Z3Expr Distinct(Z3Expr term) const;
	};


	class Z3Encoder final : public Encoder, public std::enable_shared_from_this<Z3Encoder> {
		public: // implement Encoder interface
			explicit Z3Encoder(std::shared_ptr<SolverConfig> config);
			static inline Z3Expr Internalize(Term term) { return Z3Expr(std::move(term)); }
			static inline Z3Expr Internalize(Symbol symbol) { return Z3Expr(std::move(symbol)); }

			[[nodiscard]] Term MakeNullPtr() override;
			[[nodiscard]] Term MakeMinValue() override;
			[[nodiscard]] Term MakeMaxValue() override;
			[[nodiscard]] Term MakeBool(bool value) override;

			[[nodiscard]] Term MakeOr(const std::vector<Term>& disjuncts) override;
			[[nodiscard]] Term MakeAnd(const std::vector<Term>& conjuncts) override;
			[[nodiscard]] Term MakeAtMostOne(const std::vector<Term>& elements) override;
			[[nodiscard]] Term MakeExists(const std::vector<Symbol>& vars, Term term) override;
			[[nodiscard]] Term MakeForall(const std::vector<Symbol>& vars, Term term) override;

            [[nodiscard]] Term MakeDataBounds(Term term) override;
            [[nodiscard]] std::unique_ptr<ImplicationChecker> MakeImplicationChecker(EncodingTag tag) override;

			[[nodiscard]] Symbol Encode(const cola::VariableDeclaration& decl, EncodingTag tag) override;
			[[nodiscard]] Term Encode(const heal::Formula& formula, EncodingTag tag) override;
			[[nodiscard]] Term Encode(const cola::Expression& expression, EncodingTag tag) override;
			[[nodiscard]] Term EncodeFlow(Term node, Term value, EncodingTag tag) override;
			[[nodiscard]] Term EncodeHeap(Term node, Selector selector, EncodingTag tag) override;
			[[nodiscard]] Term EncodeHeapIs(Term node, Selector selector, Term value, EncodingTag tag) override;
			[[nodiscard]] Term EncodeHasFlow(Term node, EncodingTag tag) override;
			[[nodiscard]] Term EncodeIsOwned(Term node, EncodingTag tag) override;
			[[nodiscard]] Term EncodeUniqueInflow(Term node, EncodingTag tag) override;
			[[nodiscard]] Term EncodeKeysetContains(Term node, Term value, EncodingTag tag) override;
			[[nodiscard]] Term EncodeObligation(heal::SpecificationAxiom::Kind kind, Term value, EncodingTag tag) override;
			[[nodiscard]] Term EncodeFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue, EncodingTag tag) override;
			[[nodiscard]] Term EncodeInvariant(const heal::Invariant& invariant, Term arg, EncodingTag tag) override;
			[[nodiscard]] Term EncodePredicate(const heal::Predicate& predicate, Term arg1, Term arg2, EncodingTag tag) override;

			[[nodiscard]] Term EncodeTransitionMaintainsOwnership(Term node) override;
			[[nodiscard]] Term EncodeTransitionMaintainsFlow(Term node, Term key) override;
			[[nodiscard]] Term EncodeTransitionMaintainsHeap(Term node, Selector selector) override;

		public: // Encoder interface for downcast'd terms/symbols
			[[nodiscard]] Z3Expr MakeZ3NullPtr();
			[[nodiscard]] Z3Expr MakeZ3MinValue();
			[[nodiscard]] Z3Expr MakeZ3MaxValue();
			[[nodiscard]] Z3Expr MakeZ3Bool(bool value);
			[[nodiscard]] inline Z3Expr MakeZ3True() { return MakeZ3Bool(true); }
			[[nodiscard]] inline Z3Expr MakeZ3False() { return MakeZ3Bool(false); }
			[[nodiscard]] Z3Expr MakeZ3DataBounds(Z3Expr value);

			[[nodiscard]] Z3Expr MakeZ3Or(const std::vector<Z3Expr>& disjuncts);
			[[nodiscard]] Z3Expr MakeZ3And(const std::vector<Z3Expr>& conjuncts);
			[[nodiscard]] Z3Expr MakeZ3AtMostOne(const std::vector<Z3Expr>& elements);
			[[nodiscard]] Z3Expr MakeZ3Exists(const std::vector<Z3Expr>& vars, Z3Expr term);
			[[nodiscard]] Z3Expr MakeZ3Forall(const std::vector<Z3Expr>& vars, Z3Expr term);

			[[nodiscard]] Z3Expr EncodeZ3(const cola::VariableDeclaration& decl, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::Formula& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::Expression& expression, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3Flow(Z3Expr node, Z3Expr value, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3Heap(Z3Expr node, Selector selector, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3HeapIs(Z3Expr node, Selector selector, Z3Expr value, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3HasFlow(Z3Expr node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3IsOwned(Z3Expr node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3UniqueInflow(Z3Expr node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3KeysetContains(Z3Expr node, Z3Expr value, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3Obligation(heal::SpecificationAxiom::Kind kind, Z3Expr value, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3Fulfillment(heal::SpecificationAxiom::Kind kind, Z3Expr value, bool returnValue, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3Invariant(const heal::Invariant& invariant, Z3Expr arg, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3Predicate(const heal::Predicate& predicate, Z3Expr arg1, Z3Expr arg2, EncodingTag tag);

			[[nodiscard]] Z3Expr EncodeZ3TransitionMaintainsOwnership(Z3Expr node);
			[[nodiscard]] Z3Expr EncodeZ3TransitionMaintainsFlow(Z3Expr node, Z3Expr key);
			[[nodiscard]] Z3Expr EncodeZ3TransitionMaintainsHeap(Z3Expr node, Selector selector);

		public: // visitor Interface for encoding
			[[nodiscard]] Z3Expr EncodeZ3(const cola::BooleanValue& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::NullValue& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::EmptyValue& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::MaxValue& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::MinValue& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::NDetValue& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::VariableExpression& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::NegatedExpression& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::BinaryExpression& node, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const cola::Dereference& node, EncodingTag tag);

			[[nodiscard]] Z3Expr EncodeZ3(const heal::ConjunctionFormula& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::NegationFormula& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::ExpressionAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::ImplicationFormula& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::OwnershipAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::DataStructureLogicallyContainsAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::NodeLogicallyContainsAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::KeysetContainsAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::HasFlowAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::FlowContainsAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::UniqueInflowAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::ObligationAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::FulfillmentAxiom& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::PastPredicate& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::FuturePredicate& formula, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3(const heal::Annotation& formula, EncodingTag tag);

		private: // helpers
			[[nodiscard]] z3::sort EncodeZ3Sort(cola::Sort sort);
			[[nodiscard]] Z3Expr EncodeZ3Spec(heal::SpecificationAxiom::Kind kind);
			[[nodiscard]] Z3Expr EncodeZ3Variable(cola::Sort sort, std::string name, EncodingTag tag);
			[[nodiscard]] Z3Expr EncodeZ3Selector(Selector selector, EncodingTag tag);

		public: // helpers
			[[nodiscard]] z3::expr_vector MakeZ3ExprVector();
			[[nodiscard]] z3::expr_vector MakeZ3ExprVector(const std::vector<Z3Expr>& vector);

		private: // fields
			template<typename T> using expr_map_t = std::map<T, z3::expr>;
			template<typename T> using tagged_expr_map_t = std::map<std::pair<T, EncodingTag>, z3::expr>;

			tagged_expr_map_t<const cola::VariableDeclaration*> decl2expr;
			tagged_expr_map_t<std::string> name2expr;
			expr_map_t<Selector> selector2expr;
			int selector_count = 0;

			z3::context context;
			z3::sort ptrSort, dataSort, valueSort, boolSort, selectorSort, specSort; // valueSort = ptrSort + dataSort
			z3::expr nullPtr, minVal, maxVal;
			z3::func_decl heapNow, heapNext; // free function: Ptr x Sel -> Val; 'heap(x, sel) = y' means that field 'sel' of node 'x' is 'y'
			z3::func_decl flowNow, flowNext; // free function: Ptr x Data -> Bool; 'flow(x, k) = true' iff the flow in node 'x' contains 'k'
			z3::func_decl uniqueInflowNow, uniqueInflowNext; // free function: Ptr -> Bool; 'uniqueInflow(x) = true' iff node 'x' receives any key 'k' from at most 1 node
			z3::func_decl ownershipNow, ownershipNext; // free function: Ptr -> Bool; 'ownership(x) = true' iff 'x' is owned
			z3::func_decl obligationNow, obligationNext; // free function: Data x Spec -> Bool; 'obligation(k, i) = true' iff 'OBL(k, i)'
			z3::func_decl fulfillmentNow, fulfillmentNext; // free function: Data x Spec x Bool -> Bool; 'fulfillment(k, i, r) = true' iff 'FUL(k, i, r)'

			std::vector<std::pair<std::string, const cola::Type*>> pointerFields;
	};

} // namespace engine

#endif