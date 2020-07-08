#pragma once
#ifndef PLANKTON_ENCODER
#define PLANKTON_ENCODER


#include <list>
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
			enum StepTag { NOW, NEXT };
			Encoder(const PostConfig& config);

			z3::expr Encode(const Formula& formula, StepTag tag = NOW);
			z3::expr Encode(const cola::Expression& expression, StepTag tag = NOW);
			inline z3::expr EncodeNext(const Formula& formula) { return Encode(formula, NEXT); }
			inline z3::expr EncodeNext(const cola::Expression& expression) { return Encode(expression, NEXT); }

			z3::solver MakeSolver();

			z3::expr MakeNullPtr();
			z3::expr MakeMinValue();
			z3::expr MakeMaxValue();

			z3::expr MakeBool(bool value);
			z3::expr MakeTrue();
			z3::expr MakeFalse();

			z3::expr MakeOr(const z3::expr_vector& disjuncts);
			z3::expr MakeAnd(const z3::expr_vector& conjuncts);
			z3::expr MakeImplication(z3::expr premise, z3::expr conclusion);

		private:
			using selector_t = std::pair<const cola::Type*, std::string>;
			template<typename T> using expr_map_t = std::map<T, z3::expr>;
			template<typename T> using tagged_expr_map_t = std::map<std::pair<T, StepTag>, z3::expr>;

			tagged_expr_map_t<const cola::VariableDeclaration*> decl2expr;
			tagged_expr_map_t<std::string> name2expr;
			expr_map_t<selector_t> selector2expr;
			int selector_count = 0;

			z3::context context;
			z3::sort intSort, boolSort;
			z3::expr nullPtr, minVal, maxVal;
			z3::func_decl heap; // free function: Int x Int -> Int; 'heap(x, sel) = y' means that field 'sel' of node 'x' is 'y'
			z3::func_decl flow; // free function: Int x Int -> Bool; 'flow(x, k) = true' iff the flow in node 'x' contains 'k'
			z3::func_decl ownership; // free function: Int -> Bool; 'ownership(x) = true' iff 'x' is owned

			const PostConfig& postConfig;

		private:
			z3::sort EncodeSort(StepTag tag, cola::Sort sort);
			z3::expr EncodeVariable(StepTag tag, cola::Sort sort, std::string name);
			z3::expr EncodeVariable(StepTag tag, const cola::VariableDeclaration& decl);
			z3::expr EncodeSelector(StepTag tag, selector_t selector);
			z3::expr EncodeHeap(StepTag tag, z3::expr pointer, selector_t selector);
			z3::expr EncodeHeap(StepTag tag, z3::expr pointer, selector_t selector, z3::expr value);
			z3::expr EncodeFlow(StepTag tag, z3::expr pointer, z3::expr value, bool containsValue=true);
			z3::expr EncodeOwnership(StepTag tag, z3::expr pointer, bool owned=true);
			z3::expr EncodeHasFlow(StepTag tag, z3::expr node);
			z3::expr EncodePredicate(StepTag tag, const Predicate& predicate, z3::expr arg1, z3::expr arg2);
			z3::expr EncodeKeysetContains(StepTag tag, z3::expr node, z3::expr key);

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
			z3::expr Encode(StepTag tag, const LogicallyContainedAxiom& formula);
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
		void visit(const LogicallyContainedAxiom& formula) override { result = encoder.Encode(tag, formula); }
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