#pragma once
#ifndef PLANKTON_ENCODER
#define PLANKTON_ENCODER


#include <set>
#include <deque>
#include <vector>
#include "z3++.h"
#include "cola/ast.hpp"
#include "cola/visitors.hpp"
#include "plankton/logic.hpp"
#include "plankton/solver.hpp"


namespace plankton {

	// TODO: allow for parallel solving
	// NOTE: for using multiple solver is parallel, the context needs to be duplicated as well; and expressions need to be converted among contexts
	//       for copying a solver, see: https://z3prover.github.io/api/html/classz3_1_1solver.html#a17c7db98d5ca41022de0d37ae6623aeb

	class Encoder {
		public:
			using selector_t = std::pair<const cola::Type*, std::string>;
			enum struct StepTag { NOW, NEXT };
			Encoder(const PostConfig& config);

			z3::expr Encode(const Formula& formula, StepTag tag = StepTag::NOW);
			z3::expr Encode(const cola::Expression& expression, StepTag tag = StepTag::NOW);
			z3::expr EncodeHeap(z3::expr pointer, selector_t selector, StepTag tag = StepTag::NOW);
			z3::expr EncodeHeap(z3::expr pointer, selector_t selector, z3::expr value, StepTag tag = StepTag::NOW);
			z3::expr EncodeFlow(z3::expr pointer, z3::expr value, bool containsValue=true, StepTag tag = StepTag::NOW);
			z3::expr EncodeHasFlow(z3::expr node, StepTag tag = StepTag::NOW);
			z3::expr EncodeVariable(const cola::VariableDeclaration& decl, StepTag tag = StepTag::NOW);
			z3::expr EncodeOwnership(z3::expr pointer, bool owned=true, StepTag tag = StepTag::NOW);
			z3::expr EncodeKeysetContains(z3::expr node, z3::expr key, StepTag tag = StepTag::NOW);
			z3::expr EncodeObligation(SpecificationAxiom::Kind kind, z3::expr key, StepTag tag = StepTag::NOW);
			z3::expr EncodeFulfillment(SpecificationAxiom::Kind kind, z3::expr key, bool returnValue, StepTag tag = StepTag::NOW);
			z3::expr EncodeInvariant(const Invariant& invariant, z3::expr arg, StepTag tag = StepTag::NOW);
			z3::expr EncodePredicate(const Predicate& predicate, z3::expr arg1, z3::expr arg2, StepTag tag = StepTag::NOW);

			inline z3::expr EncodeNext(const Formula& formula) { return Encode(formula, StepTag::NEXT); }
			inline z3::expr EncodeNext(const cola::Expression& expression) { return Encode(expression, StepTag::NEXT); }
			inline z3::expr EncodeNextHeap(z3::expr pointer, selector_t selector) { return EncodeHeap(pointer, selector, StepTag::NEXT); }
			inline z3::expr EncodeNextHeap(z3::expr pointer, selector_t selector, z3::expr value) { return EncodeHeap(pointer, selector, value, StepTag::NEXT); }
			inline z3::expr EncodeNextFlow(z3::expr pointer, z3::expr value, bool containsValue=true) { return EncodeFlow(pointer, value, containsValue, StepTag::NEXT); }
			inline z3::expr EncodeNextHasFlow(z3::expr node) { return EncodeHasFlow(node, StepTag::NEXT); }
			inline z3::expr EncodeNextVariable(const cola::VariableDeclaration& decl) { return EncodeVariable(decl, StepTag::NEXT); }
			inline z3::expr EncodeNextOwnership(z3::expr pointer, bool owned=true) { return EncodeOwnership(pointer, owned, StepTag::NEXT); }
			inline z3::expr EncodeNextKeysetContains(z3::expr node, z3::expr key) { return EncodeKeysetContains(node, key, StepTag::NEXT); }
			inline z3::expr EncodeNextObligation(SpecificationAxiom::Kind kind, z3::expr key) { return EncodeObligation(kind, key, StepTag::NEXT); }
			inline z3::expr EncodeNextFulfillment(SpecificationAxiom::Kind kind, z3::expr key, bool returnValue) { return EncodeFulfillment(kind, key, returnValue, StepTag::NEXT); }
			inline z3::expr EncodeNextInvariant(const Invariant& invariant, z3::expr arg) { return EncodeInvariant(invariant, arg, StepTag::NEXT); }
			inline z3::expr EncodeNextPredicate(const Predicate& predicate, z3::expr arg1, z3::expr arg2) { return EncodePredicate(predicate, arg1, arg2, StepTag::NEXT); }

			z3::expr EncodeTransitionMaintainsHeap(z3::expr node, const cola::Type& nodeType, std::set<std::string> excludedSelectors = {});
			z3::expr EncodeTransitionMaintainsFlow(z3::expr node, z3::expr key);
			z3::expr EncodeTransitionMaintainsOwnership(z3::expr node);

			z3::solver MakeSolver(StepTag tag = StepTag::NOW);
			std::pair<z3::solver, z3::expr_vector> MakePostSolver(std::size_t footPrintSize);

			z3::expr MakeNullPtr();
			z3::expr MakeMinValue();
			z3::expr MakeMaxValue();

			z3::expr MakeBool(bool value);
			z3::expr MakeTrue();
			z3::expr MakeFalse();

			z3::expr MakeOr(const z3::expr_vector& disjuncts);
			z3::expr MakeAnd(const z3::expr_vector& conjuncts);
			z3::expr MakeImplication(z3::expr premise, z3::expr conclusion);
			z3::expr MakeEquivalence(z3::expr expression, z3::expr other);
			z3::expr_vector MakeVector();

		private:
			template<typename T> using expr_map_t = std::map<T, z3::expr>;
			template<typename T> using tagged_expr_map_t = std::map<std::pair<T, StepTag>, z3::expr>;

			tagged_expr_map_t<const cola::VariableDeclaration*> decl2expr;
			tagged_expr_map_t<std::string> name2expr;
			expr_map_t<selector_t> selector2expr;
			int selector_count = 0;

			z3::context context;
			z3::sort intSort, boolSort;
			z3::expr nullPtr, minVal, maxVal;
			z3::func_decl heapNow, heapNext; // free function: Int x Int -> Int; 'heap(x, sel) = y' means that field 'sel' of node 'x' is 'y'
			z3::func_decl flowNow, flowNext; // free function: Int x Int -> Bool; 'flow(x, k) = true' iff the flow in node 'x' contains 'k'
			z3::func_decl ownershipNow, ownershipNext; // free function: Int -> Bool; 'ownership(x) = true' iff 'x' is owned
			z3::func_decl obligationNow, obligationNext; // free function: Int x Int -> Bool; 'obligation(k, i) = true' iff 'OBL(k, i)'
			z3::func_decl fulfillmentNow, fulfillmentNext; // free function: Int x Int x Bool -> Bool; 'fulfillment(k, i, r) = true' iff 'FUL(k, i, r)'

			const PostConfig& postConfig;
			std::optional<std::vector<std::pair<std::string, const cola::Type*>>> pointerFields;

		private:
			const std::vector<std::pair<std::string, const cola::Type*>>& GetNodeTypePointerFields();

			z3::sort EncodeSort(cola::Sort sort);
			z3::expr EncodeSpec(SpecificationAxiom::Kind kind);
			z3::expr EncodeVariable(cola::Sort sort, std::string name, StepTag tag);
			z3::expr EncodeSelector(selector_t selector, StepTag tag);

			z3::expr Encode(StepTag tag, const cola::VariableDeclaration& node);
			z3::expr Encode(StepTag tag, const cola::BooleanValue& node);
			z3::expr Encode(StepTag tag, const cola::NullValue& node);
			z3::expr Encode(StepTag tag, const cola::EmptyValue& node);
			z3::expr Encode(StepTag tag, const cola::MaxValue& node);
			z3::expr Encode(StepTag tag, const cola::MinValue& node);
			z3::expr Encode(StepTag tag, const cola::NDetValue& node);
			z3::expr Encode(StepTag tag, const cola::VariableExpression& node);
			z3::expr Encode(StepTag tag, const cola::NegatedExpression& node);
			z3::expr Encode(StepTag tag, const cola::BinaryExpression& node);
			z3::expr Encode(StepTag tag, const cola::Dereference& node);

			z3::expr Encode(StepTag tag, const AxiomConjunctionFormula& formula);
			z3::expr Encode(StepTag tag, const ConjunctionFormula& formula);
			z3::expr Encode(StepTag tag, const NegatedAxiom& formula);
			z3::expr Encode(StepTag tag, const ExpressionAxiom& formula);
			z3::expr Encode(StepTag tag, const ImplicationFormula& formula);
			z3::expr Encode(StepTag tag, const OwnershipAxiom& formula);
			z3::expr Encode(StepTag tag, const DataStructureLogicallyContainsAxiom& formula);
			z3::expr Encode(StepTag tag, const NodeLogicallyContainsAxiom& formula);
			z3::expr Encode(StepTag tag, const KeysetContainsAxiom& formula);
			z3::expr Encode(StepTag tag, const HasFlowAxiom& formula);
			z3::expr Encode(StepTag tag, const FlowContainsAxiom& formula);
			z3::expr Encode(StepTag tag, const ObligationAxiom& formula);
			z3::expr Encode(StepTag tag, const FulfillmentAxiom& formula);
			z3::expr Encode(StepTag tag, const PastPredicate& formula);
			z3::expr Encode(StepTag tag, const FuturePredicate& formula);
			z3::expr Encode(StepTag tag, const Annotation& formula);


		friend struct EncoderCallback;
	};

	struct EncoderCallback final : public cola::BaseVisitor, public BaseLogicVisitor {
		using cola::BaseVisitor::visit;
		using BaseLogicVisitor::visit;

		Encoder& encoder;
		Encoder::StepTag tag;
		std::optional<z3::expr> result;

		EncoderCallback(Encoder& encoder, Encoder::StepTag tag) : encoder(encoder), tag(tag) {}
		z3::expr GetResult() { assert(result.has_value()); return result.value(); }
		z3::expr Encode(const cola::Expression& expression) { expression.accept(*this); return GetResult(); }
		z3::expr Encode(const Formula& formula) { formula.accept(*this); return GetResult(); }

		void visit(const cola::VariableDeclaration& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::BooleanValue& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::NullValue& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::EmptyValue& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::MaxValue& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::MinValue& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::NDetValue& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::VariableExpression& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::NegatedExpression& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::BinaryExpression& node) override { result = encoder.Encode(tag, node); }
		void visit(const cola::Dereference& node) override { result = encoder.Encode(tag, node); }
		void visit(const AxiomConjunctionFormula& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const ConjunctionFormula& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const NegatedAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const ExpressionAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const ImplicationFormula& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const OwnershipAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const DataStructureLogicallyContainsAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const NodeLogicallyContainsAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const KeysetContainsAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const HasFlowAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const FlowContainsAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const ObligationAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const FulfillmentAxiom& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const PastPredicate& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const FuturePredicate& formula) override { result = encoder.Encode(tag, formula); }
		void visit(const Annotation& formula) override { result = encoder.Encode(tag, formula); }
	};

} // namespace plankton

#endif