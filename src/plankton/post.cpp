#include "plankton/post.hpp"

#include <iostream> // TODO: remove
#include <sstream>
#include <type_traits>
#include "cola/util.hpp"
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

// TODO: move this to a more appropriate location?

struct DefaultListener : public BaseVisitor, public LogicVisitor {
	using BaseVisitor::visit;
	using LogicVisitor::visit;

	virtual void visit(const BooleanValue& node) override { enter(node); exit(node); }
	virtual void visit(const NullValue& node) override { enter(node); exit(node); }
	virtual void visit(const EmptyValue& node) override { enter(node); exit(node); }
	virtual void visit(const MaxValue& node) override { enter(node); exit(node); }
	virtual void visit(const MinValue& node) override { enter(node); exit(node); }
	virtual void visit(const NDetValue& node) override { enter(node); exit(node); }
	virtual void visit(const VariableExpression& node) override { enter(node); exit(node); }
	virtual void visit(const NegatedExpression& node) override { enter(node); node.expr->accept(*this); exit(node); }
	virtual void visit(const BinaryExpression& node) override { enter(node); node.lhs->accept(*this); node.rhs->accept(*this); exit(node); }
	virtual void visit(const Dereference& node) override { enter(node); node.expr->accept(*this); exit(node); }
	virtual void visit(const ExpressionFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(const OwnershipFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(const LogicallyContainedFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(const FlowFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(const ObligationFormula& formula) override { enter(formula); throw std::logic_error("not yet implemented: DefaultListener::visit(const ObligationFormula&)"); exit(formula); }
	virtual void visit(const FulfillmentFormula& formula) override { enter(formula); throw std::logic_error("not yet implemented: DefaultListener::visit(const FulfillmentFormula&)"); exit(formula); }
	virtual void visit(const NegatedFormula& formula) override { enter(formula); formula.formula->accept(*this); exit(formula); }
	virtual void visit(const ConjunctionFormula& formula) override {
		enter(formula);
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
		exit(formula);
	}
	virtual void visit(const PastPredicate& formula) override { enter(formula); formula.formula->accept(*this); exit(formula); }
	virtual void visit(const FuturePredicate& formula) override { enter(formula); formula.pre->accept(*this); formula.post->accept(*this); exit(formula); }
	virtual void visit(const Annotation& formula) override {
		enter(formula);
		formula.now->accept(*this);
		for (const auto& past : formula.past) {
			past.accept(*this);
		}
		for (const auto& fut : formula.future) {
			fut.accept(*this);
		}
		exit(formula);
	}

	virtual void enter(const BooleanValue& /*node*/) {}
	virtual void enter(const NullValue& /*node*/) {}
	virtual void enter(const EmptyValue& /*node*/) {}
	virtual void enter(const MaxValue& /*node*/) {}
	virtual void enter(const MinValue& /*node*/) {}
	virtual void enter(const NDetValue& /*node*/) {}
	virtual void enter(const VariableExpression& /*node*/) {}
	virtual void enter(const NegatedExpression& /*node*/) {}
	virtual void enter(const BinaryExpression& /*node*/) {}
	virtual void enter(const Dereference& /*node*/) {}
	virtual void enter(const ConjunctionFormula& /*formula*/) {}
	virtual void enter(const ExpressionFormula& /*formula*/) {}
	virtual void enter(const NegatedFormula& /*formula*/) {}
	virtual void enter(const OwnershipFormula& /*formula*/) {}
	virtual void enter(const LogicallyContainedFormula& /*formula*/) {}
	virtual void enter(const FlowFormula& /*formula*/) {}
	virtual void enter(const ObligationFormula& /*formula*/) {}
	virtual void enter(const FulfillmentFormula& /*formula*/) {}
	virtual void enter(const PastPredicate& /*formula*/) {}
	virtual void enter(const FuturePredicate& /*formula*/) {}
	virtual void enter(const Annotation& /*formula*/) {}

	virtual void exit(const BooleanValue& /*node*/) {}
	virtual void exit(const NullValue& /*node*/) {}
	virtual void exit(const EmptyValue& /*node*/) {}
	virtual void exit(const MaxValue& /*node*/) {}
	virtual void exit(const MinValue& /*node*/) {}
	virtual void exit(const NDetValue& /*node*/) {}
	virtual void exit(const VariableExpression& /*node*/) {}
	virtual void exit(const NegatedExpression& /*node*/) {}
	virtual void exit(const BinaryExpression& /*node*/) {}
	virtual void exit(const Dereference& /*node*/) {}
	virtual void exit(const ConjunctionFormula& /*formula*/) {}
	virtual void exit(const ExpressionFormula& /*formula*/) {}
	virtual void exit(const NegatedFormula& /*formula*/) {}
	virtual void exit(const OwnershipFormula& /*formula*/) {}
	virtual void exit(const LogicallyContainedFormula& /*formula*/) {}
	virtual void exit(const FlowFormula& /*formula*/) {}
	virtual void exit(const ObligationFormula& /*formula*/) {}
	virtual void exit(const FulfillmentFormula& /*formula*/) {}
	virtual void exit(const PastPredicate& /*formula*/) {}
	virtual void exit(const FuturePredicate& /*formula*/) {}
	virtual void exit(const Annotation& /*formula*/) {}
};

struct DefaultNonConstListener : public BaseNonConstVisitor, public LogicNonConstVisitor {
	using BaseNonConstVisitor::visit;
	using LogicNonConstVisitor::visit;

	virtual void visit(BooleanValue& node) override { enter(node); exit(node); }
	virtual void visit(NullValue& node) override { enter(node); exit(node); }
	virtual void visit(EmptyValue& node) override { enter(node); exit(node); }
	virtual void visit(MaxValue& node) override { enter(node); exit(node); }
	virtual void visit(MinValue& node) override { enter(node); exit(node); }
	virtual void visit(NDetValue& node) override { enter(node); exit(node); }
	virtual void visit(VariableExpression& node) override { enter(node); exit(node); }
	virtual void visit(NegatedExpression& node) override { enter(node); node.expr->accept(*this); exit(node); }
	virtual void visit(BinaryExpression& node) override { enter(node); node.lhs->accept(*this); node.rhs->accept(*this); exit(node); }
	virtual void visit(Dereference& node) override { enter(node); node.expr->accept(*this); exit(node); }
	virtual void visit(ExpressionFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(OwnershipFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(LogicallyContainedFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(FlowFormula& formula) override { enter(formula); formula.expr->accept(*this); exit(formula); }
	virtual void visit(NegatedFormula& formula) override { enter(formula); formula.formula->accept(*this); exit(formula); }
	virtual void visit(ConjunctionFormula& formula) override {
		enter(formula);
		for (auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
		exit(formula);
	}
	virtual void visit(PastPredicate& formula) override { enter(formula); formula.formula->accept(*this); exit(formula); }
	virtual void visit(FuturePredicate& formula) override { enter(formula); formula.pre->accept(*this); formula.post->accept(*this); exit(formula); }
	virtual void visit(Annotation& formula) override {
		enter(formula);
		formula.now->accept(*this);
		for (auto& past : formula.past) {
			past.accept(*this);
		}
		for (auto& fut : formula.future) {
			fut.accept(*this);
		}
		exit(formula);
	}

	virtual void enter(BooleanValue& /*node*/) {}
	virtual void enter(NullValue& /*node*/) {}
	virtual void enter(EmptyValue& /*node*/) {}
	virtual void enter(MaxValue& /*node*/) {}
	virtual void enter(MinValue& /*node*/) {}
	virtual void enter(NDetValue& /*node*/) {}
	virtual void enter(VariableExpression& /*node*/) {}
	virtual void enter(NegatedExpression& /*node*/) {}
	virtual void enter(BinaryExpression& /*node*/) {}
	virtual void enter(Dereference& /*node*/) {}
	virtual void enter(ConjunctionFormula& /*formula*/) {}
	virtual void enter(ExpressionFormula& /*formula*/) {}
	virtual void enter(NegatedFormula& /*formula*/) {}
	virtual void enter(OwnershipFormula& /*formula*/) {}
	virtual void enter(LogicallyContainedFormula& /*formula*/) {}
	virtual void enter(FlowFormula& /*formula*/) {}
	virtual void enter(PastPredicate& /*formula*/) {}
	virtual void enter(FuturePredicate& /*formula*/) {}
	virtual void enter(Annotation& /*formula*/) {}

	virtual void exit(BooleanValue& /*node*/) {}
	virtual void exit(NullValue& /*node*/) {}
	virtual void exit(EmptyValue& /*node*/) {}
	virtual void exit(MaxValue& /*node*/) {}
	virtual void exit(MinValue& /*node*/) {}
	virtual void exit(NDetValue& /*node*/) {}
	virtual void exit(VariableExpression& /*node*/) {}
	virtual void exit(NegatedExpression& /*node*/) {}
	virtual void exit(BinaryExpression& /*node*/) {}
	virtual void exit(Dereference& /*node*/) {}
	virtual void exit(ConjunctionFormula& /*formula*/) {}
	virtual void exit(ExpressionFormula& /*formula*/) {}
	virtual void exit(NegatedFormula& /*formula*/) {}
	virtual void exit(OwnershipFormula& /*formula*/) {}
	virtual void exit(LogicallyContainedFormula& /*formula*/) {}
	virtual void exit(FlowFormula& /*formula*/) {}
	virtual void exit(PastPredicate& /*formula*/) {}
	virtual void exit(FuturePredicate& /*formula*/) {}
	virtual void exit(Annotation& /*formula*/) {}
};


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: (1) destroy knowledge about current lhs (and everything that is not guaranteed to survive the assignment)
// TODO: (2) add new knowledge
// TODO: (3) infer new knowledge (exhaustively)

struct VariableSearcher : public DefaultListener {
	const VariableDeclaration& decl;
	bool result = false;

	VariableSearcher(const VariableDeclaration& decl_) : decl(decl_) {}

	void enter(const VariableExpression& node) override {
		if (&node.decl == &decl) {
			result = true;
		}
	}
};

// template<typename T>
// bool contains_var(const VariableDeclaration& var, const T& obj) {
// 	VariableSearcher visitor(var);
// 	obj.accept(visitor);
// 	return visitor.result;
// }

bool contains_expr(const Expression& /*formula*/, const Expression& /*search*/) {
	throw std::logic_error("not yet implemented: contains_expr");
}

bool contains_expr(const Formula& /*formula*/, const Expression& /*search*/) {
	throw std::logic_error("not yet implemented: contains_expr");
}

bool contains_expr(const std::unique_ptr<BasicFormula>& formula, const Expression& search) {
	return contains_expr(*formula, search);
}

bool contains_expr(const std::unique_ptr<Formula>& formula, const Expression& search) {
	return contains_expr(*formula, search);
}

bool contains_expr(const PastPredicate& pred, const Expression& search) {
	return contains_expr(pred.formula, search);
}

bool contains_expr(const FuturePredicate& pred, const Expression& search) {
	return contains_expr(pred.pre, search) || contains_expr(pred.post, search)
		|| contains_expr(*pred.command->lhs, search) || contains_expr(*pred.command->rhs, search);
}

std::unique_ptr<Expression> make_renamed_copy(const Expression& /*expr*/, const Expression& /*replace*/, const Expression& /*with*/) {
	throw std::logic_error("not yet implemented: make_renamed_copy");
}

std::unique_ptr<BasicFormula> make_renamed_copy(const std::unique_ptr<BasicFormula>& /*formula*/, const Expression& /*replace*/, const Expression& /*with*/) {
	throw std::logic_error("not yet implemented: make_renamed_copy");
}

std::unique_ptr<Formula> make_renamed_copy(const std::unique_ptr<Formula>& /*formula*/, const Expression& /*replace*/, const Expression& /*with*/) {
	throw std::logic_error("not yet implemented: make_renamed_copy");
}

PastPredicate make_renamed_copy(const PastPredicate& pred, const Expression& replace, const Expression& with) {
	auto copy = make_renamed_copy(pred.formula, replace, with);
	return PastPredicate(std::move(copy));
}

FuturePredicate make_renamed_copy(const FuturePredicate& pred, const Expression& replace, const Expression& with) {
	return FuturePredicate(
		make_renamed_copy(pred.pre, replace, with),
		std::make_unique<Assignment>(make_renamed_copy(*pred.command->lhs, replace, with), make_renamed_copy(*pred.command->rhs, replace, with)),
		make_renamed_copy(pred.post, replace, with)
	);
}

template<typename T>
void remove_and_replace_deque(std::deque<T>& deque, const Expression& lhs, const Expression& rhs) {
	// (1) remove knowledge about 'lhs' in 'pre'
	// (2) copy knowledge about 'rhs' and therin replace 'rhs' with 'lhs'

	std::deque<T> new_deque;
	for (auto& elem : deque) {
		if (contains_expr(elem, lhs)) {
			continue;
		}
		if (contains_expr(elem, rhs)) {
			new_deque.push_back(make_renamed_copy(elem, rhs, lhs));
		}
		new_deque.push_back(std::move(elem));
	}
	deque = std::move(new_deque);
}

std::unique_ptr<Annotation> remove_and_replace(std::unique_ptr<Annotation> pre, const Expression& lhs, const Expression& rhs) {
	// remove destroyed knowledge, copy existing knowledge over
	remove_and_replace_deque(pre->now->conjuncts, lhs, rhs);
	remove_and_replace_deque(pre->past, lhs, rhs); // TODO important: are those really duplicable
	remove_and_replace_deque(pre->future, lhs, rhs); // TODO important: are those really duplicable

	// add new knowledge
	auto equality = std::make_unique<ExpressionFormula>(std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs)));
	pre->now->conjuncts.push_back(std::move(equality));

	// done
	return pre;
}

std::unique_ptr<Annotation> post_full_assign_var_expr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Expression& rhs) {
	return remove_and_replace(std::move(pre), lhs, rhs);
}

std::unique_ptr<Annotation> post_full_assign_var_derefptr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/) {
	auto post = remove_and_replace(std::move(pre), lhs, rhs);
	// TODO: extend post with new knowledge, in particular infer knowledge about the flow and potentially useful past predicates
	throw std::logic_error("not yet implemented: post_full_assign_var_derefptr");
}

std::unique_ptr<Annotation> post_full_assign_var_derefdata(std::unique_ptr<Annotation> /*pre*/, const Assignment& /*cmd*/, const VariableExpression& /*lhs*/, const Dereference& /*rhs*/, const VariableExpression& /*rhsVar*/) {
	throw std::logic_error("not yet implemented: post_full_assign_var_derefdata");
}

std::unique_ptr<Annotation> post_full_assign_derefptr_varimmi(std::unique_ptr<Annotation> /*pre*/, const Assignment& /*cmd*/, const Dereference& /*lhs*/, const VariableExpression& /*lhsVar*/, const Expression& /*rhs*/) {
	throw std::logic_error("not yet implemented: post_full_assign_derefptr_var");
}

std::unique_ptr<Annotation> post_full_assign_derefdata_varimmi(std::unique_ptr<Annotation> /*pre*/, const Assignment& /*cmd*/, const Dereference& /*lhs*/, const VariableExpression& /*lhsVar*/, const Expression& /*rhs*/) {
	throw std::logic_error("not yet implemented: post_full_assign_derefdata_var");
}

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

bool plankton::post_maintains_formula(const ConjunctionFormula& /*pre*/, const ConjunctionFormula& /*maintained*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_formula(const ConjunctionFormula&, const ConjunctionFormula&, const cola::Assignment&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool plankton::post_maintains_invariant(const Annotation& /*pre*/, const ConjunctionFormula& /*invariant*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_invariant(const Annotation&, const ConjunctionFormula&, const cola::Assignment&)");
}
