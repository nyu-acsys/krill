#pragma once
#ifndef PLANKTON_BACKEND_Z3ENCODER
#define PLANKTON_BACKEND_Z3ENCODER


#include <memory>
#include "z3++.h"
#include "plankton/solver.hpp"
#include "plankton/encoder.hpp"
#include "plankton/backend/z3error.hpp"


namespace plankton {

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
		inline std::shared_ptr<EncodedTerm> ToTerm() const override { return std::make_shared<Z3Expr>(*this); }
		bool operator==(const EncodedTerm& other) const override;
		bool operator==(const EncodedSymbol& other) const override;
		bool operator==(const Z3Expr& other) const;
		
		void Print(std::ostream& stream) const override;
		std::shared_ptr<EncodedTerm> Negate() const override;
		std::shared_ptr<EncodedTerm> And(std::shared_ptr<EncodedTerm> term) const override;
		std::shared_ptr<EncodedTerm> Or(std::shared_ptr<EncodedTerm> term) const override;
		std::shared_ptr<EncodedTerm> XOr(std::shared_ptr<EncodedTerm> term) const override;
		std::shared_ptr<EncodedTerm> Implies(std::shared_ptr<EncodedTerm> term) const override;
		std::shared_ptr<EncodedTerm> Iff(std::shared_ptr<EncodedTerm> term) const override;
		std::shared_ptr<EncodedTerm> Equal(std::shared_ptr<EncodedTerm> term) const override;
		std::shared_ptr<EncodedTerm> Distinct(std::shared_ptr<EncodedTerm> term) const override;

		Z3Expr Neg() const;
		Z3Expr And(Z3Expr term) const;
		Z3Expr Or(Z3Expr term) const;
		Z3Expr XOr(Z3Expr term) const;
		Z3Expr Implies(Z3Expr term) const;
		Z3Expr Iff(Z3Expr term) const;
		Z3Expr Equal(Z3Expr term) const;
		Z3Expr Distinct(Z3Expr term) const;
	};


	class Z3Encoder final : public Encoder, public std::enable_shared_from_this<Z3Encoder> {
		public: // implement Encoder interface
			Z3Encoder(const PostConfig& config);
			inline Z3Expr Internalize(Term term) { return Z3Expr(term); }
			inline Z3Expr Internalize(Symbol symbol) { return Z3Expr(symbol); }

			Term MakeNullPtr() override;
			Term MakeMinValue() override;
			Term MakeMaxValue() override;
			Term MakeBool(bool value) override;

			Term MakeOr(const std::vector<Term>& disjuncts) override;
			Term MakeAnd(const std::vector<Term>& conjuncts) override;
			Term MakeAtMostOne(const std::vector<Term>& elements) override;
			Term MakeExists(const std::vector<Symbol>& vars, Term term) override;
			Term MakeForall(const std::vector<Symbol>& vars, Term term) override;
		
			Term MakeDataBounds(Term term) override;
			std::unique_ptr<ImplicationChecker> MakeImplicationChecker(EncodingTag tag = EncodingTag::NOW) override;

			Symbol Encode(const cola::VariableDeclaration& decl, EncodingTag tag) override;
			Term Encode(const heal::Formula& formula, EncodingTag tag) override;
			Term Encode(const cola::Expression& expression, EncodingTag tag) override;
			Term EncodeFlow(Term node, Term value, EncodingTag tag) override;
			Term EncodeHeap(Term node, Selector selector, EncodingTag tag) override;
			Term EncodeHeapIs(Term node, Selector selector, Term value, EncodingTag tag) override;
			Term EncodeHasFlow(Term node, EncodingTag tag) override;
			Term EncodeIsOwned(Term node, EncodingTag tag) override;
			Term EncodeUniqueInflow(Term node, EncodingTag tag) override;
			Term EncodeKeysetContains(Term node, Term value, EncodingTag tag) override;
			Term EncodeObligation(heal::SpecificationAxiom::Kind kind, Term value, EncodingTag tag) override;
			Term EncodeFulfillment(heal::SpecificationAxiom::Kind kind, Term value, bool returnValue, EncodingTag tag) override;
			Term EncodeInvariant(const heal::Invariant& invariant, Term arg, EncodingTag tag) override;
			Term EncodePredicate(const heal::Predicate& predicate, Term arg1, Term arg2, EncodingTag tag) override;

			Term EncodeTransitionMaintainsOwnership(Term node) override;
			Term EncodeTransitionMaintainsFlow(Term node, Term key) override;
			Term EncodeTransitionMaintainsHeap(Term node, Selector selector) override;

		public: // Encoder interface for downcast'd terms/symbols
			Z3Expr MakeZ3NullPtr();
			Z3Expr MakeZ3MinValue();
			Z3Expr MakeZ3MaxValue();
			Z3Expr MakeZ3Bool(bool value);
			inline Z3Expr MakeZ3True() { return MakeZ3Bool(true); }
			inline Z3Expr MakeZ3False() { return MakeZ3Bool(false); }
			Z3Expr MakeZ3DataBounds(Z3Expr value);

			Z3Expr MakeZ3Or(const std::vector<Z3Expr>& disjuncts);
			Z3Expr MakeZ3And(const std::vector<Z3Expr>& conjuncts);
			Z3Expr MakeZ3AtMostOne(const std::vector<Z3Expr>& elements);
			Z3Expr MakeZ3Exists(const std::vector<Z3Expr>& vars, Z3Expr term);
			Z3Expr MakeZ3Forall(const std::vector<Z3Expr>& vars, Z3Expr term);

			Z3Expr EncodeZ3(const cola::VariableDeclaration& decl, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::Formula& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::Expression& expression, EncodingTag tag);
			Z3Expr EncodeZ3Flow(Z3Expr node, Z3Expr value, EncodingTag tag);
			Z3Expr EncodeZ3Heap(Z3Expr node, Selector selector, EncodingTag tag);
			Z3Expr EncodeZ3HeapIs(Z3Expr node, Selector selector, Z3Expr value, EncodingTag tag);
			Z3Expr EncodeZ3HasFlow(Z3Expr node, EncodingTag tag);
			Z3Expr EncodeZ3IsOwned(Z3Expr node, EncodingTag tag);
			Z3Expr EncodeZ3UniqueInflow(Z3Expr node, EncodingTag tag);
			Z3Expr EncodeZ3KeysetContains(Z3Expr node, Z3Expr value, EncodingTag tag);
			Z3Expr EncodeZ3Obligation(heal::SpecificationAxiom::Kind kind, Z3Expr value, EncodingTag tag);
			Z3Expr EncodeZ3Fulfillment(heal::SpecificationAxiom::Kind kind, Z3Expr value, bool returnValue, EncodingTag tag);
			Z3Expr EncodeZ3Invariant(const heal::Invariant& invariant, Z3Expr arg, EncodingTag tag);
			Z3Expr EncodeZ3Predicate(const heal::Predicate& predicate, Z3Expr arg1, Z3Expr arg2, EncodingTag tag);

			Z3Expr EncodeZ3TransitionMaintainsOwnership(Z3Expr node);
			Z3Expr EncodeZ3TransitionMaintainsFlow(Z3Expr node, Z3Expr key);
			Z3Expr EncodeZ3TransitionMaintainsHeap(Z3Expr node, Selector selector);

		public: // visitor Interface for encoding
			Z3Expr EncodeZ3(const cola::BooleanValue& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::NullValue& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::EmptyValue& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::MaxValue& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::MinValue& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::NDetValue& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::VariableExpression& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::NegatedExpression& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::BinaryExpression& node, EncodingTag tag);
			Z3Expr EncodeZ3(const cola::Dereference& node, EncodingTag tag);

			Z3Expr EncodeZ3(const heal::AxiomConjunctionFormula& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::ConjunctionFormula& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::NegatedAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::ExpressionAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::ImplicationFormula& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::OwnershipAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::DataStructureLogicallyContainsAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::NodeLogicallyContainsAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::KeysetContainsAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::HasFlowAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::FlowContainsAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::UniqueInflowAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::ObligationAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::FulfillmentAxiom& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::PastPredicate& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::FuturePredicate& formula, EncodingTag tag);
			Z3Expr EncodeZ3(const heal::Annotation& formula, EncodingTag tag);

		private: // helpers
			z3::sort EncodeZ3Sort(cola::Sort sort);
			Z3Expr EncodeZ3Spec(heal::SpecificationAxiom::Kind kind);
			Z3Expr EncodeZ3Variable(cola::Sort sort, std::string name, EncodingTag tag);
			Z3Expr EncodeZ3Selector(Selector selector, EncodingTag tag);

		public: // helpers
			z3::expr_vector MakeZ3ExprVector();
			z3::expr_vector MakeZ3ExprVector(const std::vector<Z3Expr>& vector);

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

} // namespace plankton

#endif