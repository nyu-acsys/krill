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

std::unique_ptr<Annotation> search_and_destroy_and_inline(std::unique_ptr<Annotation> pre, const Expression& lhs, const Expression& rhs) {
	// destroy knowledge about lhs
	auto flat = plankton::flatten(std::move(pre->now));
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
	throw std::logic_error("not yet implemented");
}

std::unique_ptr<Annotation> post_full_assign_var_expr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Expression& rhs) {
	return search_and_destroy_and_inline(std::move(pre), lhs, rhs);
}

std::unique_ptr<Annotation> post_full_assign_var_derefptr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/) {
	auto post = search_and_destroy_and_inline(std::move(pre), lhs, rhs);
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

bool plankton::post_maintains_formula(const Formula& /*pre*/, const Formula& /*maintained*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_formula(const Formula&, const Formula&, const cola::Assignment&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool plankton::post_maintains_invariant(const Annotation& /*pre*/, const Formula& /*invariant*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_invariant(const Annotation&, const Formula&, const cola::Assignment&)");
}
