#include "plankton/post.hpp"

#include <iostream> // TODO: remove
#include <sstream>
#include <type_traits>
#include "cola/util.hpp"
#include "plankton/util.hpp"
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> /*pre*/, const Assume& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post(std::unique_ptr<Annotation>, const Assume&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> /*pre*/, const Malloc& /*cmd*/) {
	// TODO: extend_current_annotation(... knowledge about all members of new object, flow...)
	// TODO: destroy knowledge about current lhs (and everything that is not guaranteed to survive the assignment)
	throw std::logic_error("not yet implemented: plankton::post(std::unique_ptr<Annotation>, const Malloc&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: (1) destroy knowledge about current lhs (and everything that is not guaranteed to survive the assignment)
// TODO: (2) add new knowledge
// TODO: (3) infer new knowledge (exhaustively)


struct FormChecker : public LogicVisitor {
	static void check(const Annotation& formula) {
		FormChecker visitor;
		formula.accept(visitor);
	}
	void visit(const FlowFormula& /*formula*/) override {
		throw std::logic_error("Malformed annotation: 'FlowFormula' not expected here.");
	}
	void visit(const LogicallyContainedFormula& /*formula*/) override {
		throw std::logic_error("Malformed annotation: 'LogicallyContainedFormula' not expected here.");
	}

	void visit(const ConjunctionFormula& formula) override {
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}
	void visit(const ImplicationFormula& formula) override { formula.premise->accept(*this); formula.conclusion->accept(*this); }
	void visit(const Annotation& annotation) override { annotation.now->accept(*this); }
	void visit(const NegatedFormula& formula) override { formula.formula->accept(*this); }
	void visit(const ExpressionFormula& /*formula*/) override { /* do nothing */ }
	void visit(const OwnershipFormula& /*formula*/) override { /* do nothing */ }
	void visit(const ObligationFormula& /*formula*/) override { /* do nothing */ }
	void visit(const FulfillmentFormula& /*formula*/) override { /* do nothing */ }
	void visit(const PastPredicate& /*formula*/) override { /* do nothing */ }
	void visit(const FuturePredicate& /*formula*/) override { /* do nothing */ }
};





struct OwnershipChecker : public LogicNonConstVisitor {
	const Expression& prune_expr;
	bool prune_child = false;
	bool prune_value = true;
	OwnershipChecker(const Expression& expr) : prune_expr(expr) {}

	template<typename T>
	void handle_formula(std::unique_ptr<T>& formula) {
		formula->accept(*this);
		if (prune_child) {
			formula = std::make_unique<ExpressionFormula>(std::make_unique<BooleanValue>(prune_value));
			prune_child = false;
		}
	}

	void visit(ConjunctionFormula& formula) override {
		for (auto& conjunct : formula.conjuncts) {
			handle_formula(conjunct);
		}
	}
	
	void visit(NegatedFormula& formula) override {
		bool old_value = prune_value;
		prune_value = !prune_value;
		handle_formula(formula.formula);
		prune_value = old_value;
	}

	void visit(ImplicationFormula& formula) override {
		// do not touch formula.precondition
		handle_formula(formula.conclusion);
	}

	void visit(OwnershipFormula& formula) override {
		if (plankton::syntactically_equal(*formula.expr, prune_expr)) {
			prune_child = true;
		}
	}

	void visit(ExpressionFormula& /*formula*/) override { /* do nothing */ }
	void visit(LogicallyContainedFormula& /*formula*/) override { /* do nothing */ }
	void visit(FlowFormula& /*formula*/) override { /* do nothing */ }
	void visit(ObligationFormula& /*formula*/) override { /* do nothing */ }
	void visit(FulfillmentFormula& /*formula*/) override { /* do nothing */ }
	void visit(PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: OwnershipChecker::visit(PastPredicate&)"); }
	void visit(FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: OwnershipChecker::visit(FuturePredicate&)"); }
	void visit(Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: OwnershipChecker::visit(Annotation&)"); }
};

std::unique_ptr<Formula> destroy_ownership(std::unique_ptr<Formula> formula, const Expression& expr) {
	// remove ownership of 'expr' in 'uptrcontainer'
	OwnershipChecker visitor(expr);
	visitor.handle_formula(formula);
	return formula;
}

template<typename T>
T container_search_and_destroy(T&& uptrcontainer, const Expression& destroy) {
	// remove knowledge about 'destroy' in 'uptrcontainer'
	T new_container;
	for (auto& elem : uptrcontainer) {
		if (!plankton::contains_expression(*elem, destroy)) {
			new_container.push_back(std::move(elem));
		}
	}
	return std::move(new_container);
}

template<typename T>
T container_search_and_inline(T&& uptrcontainer, const Expression& search, const Expression& replacement) {
	// copy knowledge about 'search' in 'uptrcontainer' over to 'replacement'
	T new_elements;
	for (auto& elem : uptrcontainer) {
		if (plankton::contains_expression(*elem, search)) {
			new_elements.push_back(replace_expression(plankton::copy(*elem), search, replacement));
		}
	}
	uptrcontainer.insert(uptrcontainer.end(), std::make_move_iterator(new_elements.begin()), std::make_move_iterator(new_elements.end()));
	return std::move(uptrcontainer);
}

std::unique_ptr<Annotation> search_and_destroy_and_inline_var(std::unique_ptr<Annotation> pre, const VariableExpression& lhs, const Expression& rhs) {
	// destroy knowledge about lhs
	auto pruned = destroy_ownership(std::move(pre->now), rhs);
	auto flat = plankton::flatten(std::move(pruned));
	flat->conjuncts = container_search_and_destroy(std::move(flat->conjuncts), lhs);
	pre->time = container_search_and_destroy(std::move(pre->time), lhs);

	// copy knowledge about rhs over to lhs
	flat->conjuncts = container_search_and_inline(std::move(flat->conjuncts), rhs, lhs);
	pre->time = container_search_and_inline(std::move(pre->time), rhs, lhs); // TODO important: are those really freely duplicable?

	// add new knowledge
	auto equality = std::make_unique<ExpressionFormula>(std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs)));
	flat->conjuncts.push_back(std::move(equality));
	pre->now = std::move(flat);

	// done
	return pre;
}

//////////////////////////////////////
//////////////////////////////////////

struct DereferenceExtractor : public BaseVisitor, public LogicVisitor {
	using BaseVisitor::visit;
	using LogicVisitor::visit;

	std::string search_field;
	std::deque<std::unique_ptr<Expression>> result;
	DereferenceExtractor(std::string search) : search_field(search) {}

	static std::deque<std::unique_ptr<Expression>> extract(const Formula& formula, std::string fieldname) {
		DereferenceExtractor visitor(fieldname);
		formula.accept(visitor);
		return std::move(visitor.result);
	}

	void visit(const Dereference& node) override {
		node.expr->accept(*this);
		result.push_back(cola::copy(node));
	}
	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }
	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
	void visit(const ExpressionFormula& formula) override { formula.expr->accept(*this); }
	void visit(const OwnershipFormula& formula) override { formula.expr->accept(*this); }
	void visit(const LogicallyContainedFormula& formula) override { formula.expr->accept(*this); }
	void visit(const FlowFormula& formula) override { formula.expr->accept(*this); }
	void visit(const ObligationFormula& /*formula*/) override { throw std::logic_error("not yet implemented: DereferenceExtractor::visit(const ObligationFormula&)"); }
	void visit(const FulfillmentFormula& /*formula*/) override { throw std::logic_error("not yet implemented: DereferenceExtractor::visit(const FulfillmentFormula&)"); }
	void visit(const NegatedFormula& formula) override { formula.formula->accept(*this); }
	void visit(const ConjunctionFormula& formula) override {
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}
	void visit(const ImplicationFormula& formula) override { formula.premise->accept(*this); formula.conclusion->accept(*this); }
	void visit(const PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: DereferenceExtractor::visit(const PastPredicate&)"); }
	void visit(const FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: DereferenceExtractor::visit(const FuturePredicate&)"); }
	void visit(const Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: DereferenceExtractor::visit(const Annotation&)"); }
};

std::unique_ptr<ConjunctionFormula> search_and_destroy_derefs(std::unique_ptr<ConjunctionFormula> now, const Dereference& lhs) {
	auto expr = std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::NEQ, std::make_unique<Dereference>(std::make_unique<NullValue>(), lhs.fieldname), cola::copy(lhs)
	);
	auto& uptr = expr->lhs;
	auto inequality = std::make_unique<ExpressionFormula>(std::move(expr));
	

	// find all dereferences that may be affected (are not guaranteed to be destroyed)
	auto dereferences = DereferenceExtractor::extract(*now, lhs.fieldname);
	for (auto& deref : dereferences) {
		uptr = std::move(deref);
		deref.reset();
		if (!plankton::implies(*now, *inequality)) {
			deref = std::move(uptr);
		}
	}

	// delete knowlege about potentially affected dereferences
	for (auto& deref : dereferences) {
		if (deref) {
			now->conjuncts = container_search_and_destroy(std::move(now->conjuncts), *deref);
		}
	}

	return now;
}

std::unique_ptr<Annotation> search_and_destroy_and_inline_deref(std::unique_ptr<Annotation> pre, const Dereference& lhs, const Expression& rhs) {
	// destroy knowledge about lhs (do not modify TimePredicates)
	auto pruned = destroy_ownership(std::move(pre->now), rhs);
	auto flat = plankton::flatten(std::move(pruned));
	flat = search_and_destroy_derefs(std::move(flat), lhs);
	// do not modify 

	// copy knowledge about rhs over to lhs (do not modify TimePredicates)
	flat->conjuncts = container_search_and_inline(std::move(flat->conjuncts), rhs, lhs);

	// add new knowledge
	auto equality = std::make_unique<ExpressionFormula>(std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs)));
	flat->conjuncts.push_back(std::move(equality));
	pre->now = std::move(flat);

	// done
	return pre;
}

//////////////////////////////////////
//////////////////////////////////////

std::unique_ptr<Annotation> post_full_assign_var_expr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Expression& rhs) {
	auto post = search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
	if (lhs.decl.is_shared) {
		// TODO: assignment may be impure -> obligation/fulfillment transformation requrired
		throw std::logic_error("not yet implemented: post_full_assign_var_expr");
	}
	return post;
}

std::unique_ptr<Annotation> post_full_assign_var_derefptr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/) {
	auto post = search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
	if (lhs.decl.is_shared) {
		// TODO: assignment may be impure -> obligation/fulfillment transformation requrired
		throw std::logic_error("not yet implemented: post_full_assign_var_derefptr");
	}
	// TODO: infer flow/contains and put it into a past predicate
	throw std::logic_error("not yet implemented: post_full_assign_var_derefptr");
}

std::unique_ptr<Annotation> post_full_assign_var_derefdata(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/) {
	auto post = search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
	if (lhs.decl.is_shared) {
		// TODO: assignment may be impure -> obligation/fulfillment transformation requrired
		throw std::logic_error("not yet implemented: post_full_assign_var_derefdata");
	}
	return post;
}

std::unique_ptr<Annotation> post_full_assign_derefptr_varimmi(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const Dereference& lhs, const VariableExpression& /*lhsVar*/, const Expression& rhs) {
	auto post = search_and_destroy_and_inline_deref(std::move(pre), lhs, rhs);
	// TODO: assignment may be impure -> obligation/fulfillment transformation requrired
	throw std::logic_error("not yet implemented: post_full_assign_derefptr_varimmi");
}

std::unique_ptr<Annotation> post_full_assign_derefdata_varimmi(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const Dereference& lhs, const VariableExpression& /*lhsVar*/, const Expression& rhs) {
	auto post = search_and_destroy_and_inline_deref(std::move(pre), lhs, rhs);
	// TODO: assignment may be impure -> obligation/fulfillment transformation requrired
	throw std::logic_error("not yet implemented: post_full_assign_derefdata_varimmi");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct AssignmentExpressionAnalyser : public BaseVisitor {
	const VariableExpression* decl = nullptr;
	const Dereference* deref = nullptr;
	bool derefNested = false;

	void reset() {
		decl = nullptr;
		deref = nullptr;
		derefNested = false;
	}

	void visit(const VariableExpression& node) override {
		decl = &node;
	}

	void visit(const Dereference& node) override {
		if (!deref) {
			deref = &node;
			node.expr->accept(*this);
		} else {
			derefNested = true;
		}
	}

	void visit(const NullValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const BooleanValue& /*node*/) override {if (deref) derefNested = true; }
	void visit(const EmptyValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const MaxValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const MinValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const NDetValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const NegatedExpression& /*node*/) override { if (deref) derefNested = true; }
	void visit(const BinaryExpression& /*node*/) override { if (deref) derefNested = true; }

};

inline void throw_unsupported(std::string where, const Expression& expr) {
	std::stringstream msg;
	msg << "compound expression '";
	cola::print(expr, msg);
	msg << "' as " << where << "-hand side of assignment not supported.";
	throw UnsupportedConstructError(msg.str());
}

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> pre, const Assignment& cmd) {
	std::cout << "Post for assignment:  ";
	cola::print(cmd, std::cout);
	std::cout << "under:" << std::endl;
	plankton::print(*pre, std::cout);
	std::cout << std::endl << std::endl;
	FormChecker::check(*pre);
	// TODO: make the following 🤮 nicer

	AssignmentExpressionAnalyser analyser;
	cmd.lhs->accept(analyser);
	const VariableExpression* lhsVar = analyser.decl;
	const Dereference* lhsDeref = analyser.deref;
	bool lhsDerefNested = analyser.derefNested;
	analyser.reset();
	cmd.rhs->accept(analyser);
	const VariableExpression* rhsVar = analyser.decl;
	const Dereference* rhsDeref = analyser.deref;
	bool rhsDerefNested = analyser.derefNested;

	if (lhsDerefNested || (lhsDeref == nullptr && lhsVar == nullptr)) {
		// lhs is a compound expression
		std::stringstream msg;
		msg << "compound expression '";
		cola::print(*cmd.lhs, msg);
		msg << "' as left-hand side of assignment not supported.";
		throw UnsupportedConstructError(msg.str());

	} else if (lhsDeref) {
		// lhs is a dereference
		assert(lhsVar);
		if (!rhsDeref) {
			if (lhsDeref->sort() == Sort::PTR) {
				return post_full_assign_derefptr_varimmi(std::move(pre), cmd, *lhsDeref, *lhsVar, *cmd.rhs);
			} else {
				return post_full_assign_derefdata_varimmi(std::move(pre), cmd, *lhsDeref, *lhsVar, *cmd.rhs);
			}
		} else {
			// deref on both sides not supported
			std::stringstream msg;
			msg << "right-hand side '";
			cola::print(*cmd.rhs, msg);
			msg << "' of assignment not supported with '";
			cola::print(*cmd.lhs, msg);
			msg << "' as left-hand side.";
			throw UnsupportedConstructError(msg.str());
		}

	} else if (lhsVar) {
		// lhs is a variable or an immidiate value
		if (rhsDeref) {
			if (rhsDerefNested) {
				// lhs is a compound expression
				std::stringstream msg;
				msg << "compound expression '";
				cola::print(*cmd.rhs, msg);
				msg << "' as right-hand side of assignment not supported.";
				throw UnsupportedConstructError(msg.str());

			} else if (rhsDeref->sort() == Sort::PTR) {
				return post_full_assign_var_derefptr(std::move(pre), cmd, *lhsVar, *rhsDeref, *rhsVar);
			} else {
				return post_full_assign_var_derefdata(std::move(pre), cmd, *lhsVar, *rhsDeref, *rhsVar);
			}
		} else {
			return post_full_assign_var_expr(std::move(pre), cmd, *lhsVar, *cmd.rhs);
		}
	}

	throw std::logic_error("Conditional was expected to be complete.");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool plankton::post_maintains_formula(const Formula& /*pre*/, const Formula& /*maintained*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_formula(const Formula&, const Formula&, const cola::Assignment&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool plankton::post_maintains_invariant(const Annotation& /*pre*/, const Formula& /*invariant*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_invariant(const Annotation&, const Formula&, const cola::Assignment&)");
}
