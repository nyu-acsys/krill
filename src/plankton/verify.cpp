#include "plankton/verify.hpp"

#include <sstream>
#include <iostream> // TODO: delete
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


bool plankton::check_linearizability(const Program& program) {
	Verifier verifier;
	program.accept(verifier);
	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

UnsupportedConstructError::UnsupportedConstructError(std::string construct) : VerificationError(
	"Unsupported construct: " + std::move(construct) + ".") {}

inline std::string mk_assert_msg(const Assert& cmd) {
	std::stringstream msg;
	msg << "Assertion error: ";
	cola::print(cmd, msg);
	return msg.str();
}

AssertionError::AssertionError(const Assert& cmd) : VerificationError(mk_assert_msg(cmd)) {}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::extend_interference(Effect /*effect*/) {
	// TODO: set is_interference_saturated to false if interference is changed
	throw std::logic_error("not yet implemented (extend_interference)");
}

void Verifier::apply_interference() {
	throw std::logic_error("not yet implemented (apply_interference)");
}

struct EffectSearcher : public BaseVisitor {
	bool result = false;

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }

	void visit(const VariableExpression& node) override { result |= node.decl.is_shared; }
	void visit(const Dereference& /*node*/) override { result = true; }
	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
};

bool Verifier::has_effect(const Expression& assignee) {
	// TODO: filter out assignments that do not change the assignee valuation?
	EffectSearcher searcher;
	assignee.accept(searcher);
	return searcher.result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::extend_current_annotation(std::unique_ptr<Expression> /*expr*/) {
	// current_annotation.add_conjuncts(std::move(expr));
	throw std::logic_error("not yet implemented (extend_current_annotation)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::visit(const Program& program) {
	// TODO: check initializer

	is_interference_saturated = true;
	do {
		for (const auto& func : program.functions) {
			if (func->kind == Function::Kind::INTERFACE) {
				visit_interface_function(*func);
			}
		}
	} while (!is_interference_saturated);

	throw std::logic_error("not yet implemented (Verifier::Program)");
}

void Verifier::visit_interface_function(const Function& function) {
	assert(function.kind == Function::Kind::INTERFACE);

	// TODO: get method specification and remember it?
	// TODO: add restrictions on parameters (like -inf<k<inf)? from specification?
	function.body->accept(*this);
	// TODO: check post condition entails linearizability

	throw std::logic_error("not yet implemented (Verifier::handle_interface_function)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::visit(const Sequence& stmt) {
	stmt.first->accept(*this);
	stmt.second->accept(*this);
}

void Verifier::visit(const Scope& stmt) {
	stmt.body->accept(*this);
}

void Verifier::visit(const Atomic& stmt) {
	stmt.body->accept(*this);
	apply_interference();
}

void Verifier::visit(const Choice& stmt) {
	Formula pre_condition = std::move(current_annotation);
	std::vector<Formula> post_conditions;

	for (const auto& branch : stmt.branches) {
		current_annotation = pre_condition.copy();
		branch->accept(*this);
		post_conditions.push_back(std::move(current_annotation));
	}
	
	current_annotation = unify(post_conditions);
}

void Verifier::visit(const IfThenElse& stmt) {
	Formula pre_condition = current_annotation.copy();

	// if branch
	Assume dummyIf(cola::copy(*stmt.expr));
	dummyIf.accept(*this);
	stmt.ifBranch->accept(*this);
	Formula postIf = std::move(current_annotation);

	// else branch
	current_annotation = std::move(pre_condition);
	Assume dummyElse(cola::negate(*stmt.expr));
	dummyElse.accept(*this);
	stmt.elseBranch->accept(*this);

	current_annotation = unify(current_annotation, postIf);
}

void Verifier::handle_loop(const ConditionalLoop& stmt, bool /*peelFirst*/) { // consider peelFirst a suggestion
	if (dynamic_cast<const BooleanValue*>(stmt.expr.get()) == nullptr || !dynamic_cast<const BooleanValue*>(stmt.expr.get())->value) {
		throw UnsupportedConstructError("while/do-while loop with condition other than 'true'");
	}

	std::vector<Formula> outer_breaking_annotations = std::move(breaking_annotations);
	breaking_annotations.clear();

	while (true) {
		Formula previous_annotatoin = current_annotation.copy();
		stmt.body->accept(*this);
		if (plankton::is_equal(previous_annotatoin, current_annotation)) {
			break;
		}
		current_annotation = unify(current_annotation, previous_annotatoin);
		breaking_annotations.clear();
	}

	current_annotation = unify(breaking_annotations);
	breaking_annotations = std::move(outer_breaking_annotations);
}

void Verifier::visit(const Loop& /*stmt*/) {
	throw UnsupportedConstructError("non-det loop");
}

void Verifier::visit(const While& stmt) {
	handle_loop(stmt);
}

void Verifier::visit(const DoWhile& stmt) {
	handle_loop(stmt, true);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::visit(const Skip& /*cmd*/) {
	/* do nothing */
}

void Verifier::visit(const Break& /*cmd*/) {
	breaking_annotations.push_back(std::move(current_annotation));
	current_annotation = Formula::make_false();
}

void Verifier::visit(const Continue& /*cmd*/) {
	throw UnsupportedConstructError("'continue'");
}

void Verifier::visit(const Assume& cmd) {
	extend_current_annotation(cola::copy(*cmd.expr));
}

void Verifier::visit(const Assert& cmd) {
	if (!plankton::implies(current_annotation, *cmd.expr)) {
		throw AssertionError(cmd);
	}
}

void Verifier::visit(const Return& /*cmd*/) {
	breaking_annotations.push_back(std::move(current_annotation));
	current_annotation = Formula::make_false();
}

void Verifier::visit(const Malloc& /*cmd*/) {
	// TODO: extend interference?
	// TODO: extend_current_annotation(... knowledge about all members of new object, flow...)
	throw std::logic_error("not yet implemented (Verifier::Malloc)");
}

void Verifier::handle_assignment(const Expression& lhs, const Expression& rhs) {
	auto expr = std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs));
	extend_current_annotation(std::move(expr));
}

void Verifier::visit(const Assignment& cmd) {
	if (has_effect(*cmd.lhs)) {
		extend_interference({ cola::copy(*cmd.lhs), cmd });
	}
	handle_assignment(*cmd.lhs, *cmd.rhs);
}

void Verifier::visit_macro_function(const Function& function) {
	std::vector<Formula> outer_breaking_annotations = std::move(breaking_annotations);
	std::vector<Formula> outer_returning_annotations = std::move(returning_annotations);
	breaking_annotations.clear();
	returning_annotations.clear();
	
	function.body->accept(*this);

	breaking_annotations = std::move(outer_breaking_annotations);
	returning_annotations = std::move(outer_returning_annotations);
}

void Verifier::visit(const Macro& cmd) {
	// pass arguments to function (treated as assignments)
	for (std::size_t i = 0; i < cmd.args.size(); ++i) {
		VariableExpression expr(*cmd.decl.args.at(i));
		assert(!has_effect(expr));
		handle_assignment(expr, *cmd.args.at(i));
	}

	// descend into function call
	cmd.decl.accept(*this);

	// get returned values (treated as assignments)
	for (std::size_t i = 0; i < cmd.lhs.size(); ++i) {
		VariableExpression lhs(cmd.lhs.at(i).get());
		VariableExpression rhs(*cmd.decl.returns.at(i));
		assert(!has_effect(lhs));
		handle_assignment(lhs, rhs);
	}
}

void Verifier::visit(const CompareAndSwap& /*cmd*/) {
	throw UnsupportedConstructError("CompareAndSwap");
}
