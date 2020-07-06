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

	using selector_t = std::pair<const cola::Type*, std::string>;


	void AddSolvingRulesToSolver(z3::solver solver);


	class Encoder : public cola::BaseVisitor, public BaseLogicVisitor {
		private:
			using cola::BaseVisitor::visit;
			using BaseLogicVisitor::visit;

			std::map<const cola::VariableDeclaration*, z3::expr> decl2expr;
			std::map<std::string, z3::expr> name2expr;
			std::map<selector_t, z3::expr> selector2expr;
			
			int selector_count = 0;
			int temporary_count = 0;

			z3::context context;
			z3::sort intSort, boolSort;
			z3::expr nullPtr, minVal, maxVal;
			z3::func_decl heap; // free function: Int x Int -> Int; 'heap(x, sel) = y' means that field 'sel' of node 'x' is 'y'
			z3::func_decl flow; // free function: Int x Int -> Bool; 'flow(x, k) = true' iff the flow in node 'x' contains 'k'


			z3::expr result;
			bool enhance_encoding;
			const PostConfig& postConfig;

			z3::expr EncodeInternal(const Formula& formula);
			z3::expr EncodeInternal(const cola::Expression& expression);
			z3::expr EncodeInternalHasFlow(const cola::Expression& expression);
			z3::expr EncodeInternalPredicate(const Predicate& predicate, z3::expr arg1, z3::expr arg2);
			z3::expr EncodeInternalKeysetContains(z3::expr node, z3::expr key);


		public:
			Encoder(const PostConfig& config) :
				intSort(context.int_sort()),
				boolSort(context.bool_sort()),
				nullPtr(context.constant("_NULL_", intSort)), 
				minVal(context.constant("_MIN_", intSort)),
				maxVal(context.constant("_MAX_", intSort)),
				heap(context.function("$MEM", intSort, intSort, intSort)),
				flow(context.function("$FLOW", intSort, intSort, boolSort)),
				result(MakeFalse()),
				enhance_encoding(false),
				postConfig(config)
			{
				// TODO: currently nullPtr/minVal/maxValu are just some symbols that are not bound to a value
			}


			z3::sort EncodeSort(cola::Sort sort);
			z3::expr EncodeVariable(cola::Sort sort);
			z3::expr EncodeVariable(cola::Sort sort, std::string name);
			z3::expr EncodeVariable(const cola::VariableDeclaration& decl);
			z3::expr EncodeSelector(selector_t selector);
			z3::expr_vector EncodeVector(const std::list<z3::expr>& exprs);
			z3::expr_vector EncodeVector(const std::deque<z3::expr>& exprs);
			z3::expr_vector EncodeVector(const std::vector<z3::expr>& exprs);
			z3::expr EncodeHeap(z3::expr pointer, selector_t selector);
			z3::expr EncodeFlow(z3::expr pointer, z3::expr value, bool containsValue=true);
			z3::expr EncodePremise(const Formula& formula);
			z3::expr EncodePremise(const cola::Expression& formula);
			z3::expr Encode(const Formula& formula);
			z3::expr Encode(const cola::Expression& formula);

			z3::expr GetNullPtr() { return nullPtr; }
			z3::expr GetMinValue() { return minVal; }
			z3::expr GetMaxValue() { return maxVal; }

			z3::solver MakeSolver();

			z3::expr MakeBool(bool value) { return context.bool_val(value); }
			z3::expr MakeTrue() { return MakeBool(true); }
			z3::expr MakeFalse() { return MakeBool(false); }

			z3::expr MakeImplication(z3::expr premise, z3::expr conclusion) { return z3::implies(premise, conclusion); }

			z3::expr MakeAnd(const z3::expr_vector& conjuncts);
			z3::expr MakeAnd(const std::list<z3::expr>& conjuncts);
			z3::expr MakeAnd(const std::deque<z3::expr>& conjuncts);
			z3::expr MakeAnd(const std::vector<z3::expr>& conjuncts);

			z3::expr MakeOr(const z3::expr_vector& disjuncts);
			z3::expr MakeOr(const std::list<z3::expr>& disjuncts);
			z3::expr MakeOr(const std::deque<z3::expr>& disjuncts);
			z3::expr MakeOr(const std::vector<z3::expr>& disjuncts);


			void visit(const cola::VariableDeclaration& node) override;
			void visit(const cola::BooleanValue& node) override;
			void visit(const cola::NullValue& node) override;
			void visit(const cola::EmptyValue& node) override;
			void visit(const cola::MaxValue& node) override;
			void visit(const cola::MinValue& node) override;
			void visit(const cola::NDetValue& node) override;
			void visit(const cola::VariableExpression& node) override;
			void visit(const cola::NegatedExpression& node) override;
			void visit(const cola::BinaryExpression& node) override;
			void visit(const cola::Dereference& node) override;
			void visit(const AxiomConjunctionFormula& formula) override;
			void visit(const ConjunctionFormula& formula) override;
			void visit(const NegatedAxiom& formula) override;
			void visit(const ExpressionAxiom& formula) override;
			void visit(const ImplicationFormula& formula) override;
			void visit(const OwnershipAxiom& formula) override;
			void visit(const LogicallyContainedAxiom& /*formula*/) override;
			void visit(const KeysetContainsAxiom& /*formula*/) override;
			void visit(const HasFlowAxiom& formula) override;
			void visit(const FlowContainsAxiom& formula) override;
			void visit(const ObligationAxiom& formula) override;
			void visit(const FulfillmentAxiom& formula) override;
	};

} // namespace plankton

#endif