#include "plankton/verify.hpp"

#include <set>
#include <sstream>
#include <iostream> // TODO:remove
#include "cola/util.hpp"
#include "plankton/config.hpp"
#include "plankton/post.hpp"

using namespace cola;
using namespace plankton;


bool plankton::check_linearizability(const Program& program) {
	Verifier verifier;
	program.accept(verifier);
	return true;
}


inline bool contains_name(std::string name, RenamingInfo& info) {
	for (const auto& decl : info.renamed_variables) {
		if (decl->name == name) {
			return true;
		}
	}
	return false;
}

inline std::string find_renaming(std::string name, RenamingInfo& info) {
	std::size_t counter = 0;
	std::string result;
	do {
		counter++;
		result = std::to_string(counter) + "#" + name;
	} while (contains_name(result, info));
	return result;
}

const VariableDeclaration& RenamingInfo::rename(const VariableDeclaration& decl) {
	// renames non-shared variables
	if (decl.is_shared) return decl;
	const VariableDeclaration* replacement;

	auto find = variable2renamed.find(&decl);
	if (find != variable2renamed.end()) {
		// use existing replacement
		replacement = find->second;

	} else {
		// create new replacement
		auto newName = find_renaming(decl.name, *this);
		auto newDecl = std::make_unique<VariableDeclaration>(newName, decl.type, decl.is_shared);
		variable2renamed[&decl] = newDecl.get();
		replacement = newDecl.get();
		renamed_variables.push_back(std::move(newDecl));
	}

	return *replacement;
}

transformer_t RenamingInfo::as_transformer() {
	throw std::logic_error("not yet implemented: RenamingInfo::as_transformer()");
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct VariableCollector : BaseVisitor {
	std::set<const Expression*> exprs;

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }

	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }

	void visit(const Dereference& node) override {
		exprs.insert(node.expr.get());
		node.expr->accept(*this);
	}
};

void Verifier::check_pointer_accesses(const Expression& expr) {
	VariableCollector collector;
	expr.accept(collector);
	for (const Expression* expr : collector.exprs) {
		if (!plankton::implies_nonnull(*current_annotation->now, *expr)) {
			std::stringstream msg;
			msg << "Unsafe dereference: '";
			cola::print(*expr, msg);
			msg << "' might be 'NULL'.";
			throw VerificationError(msg.str());
		}
	}
}

void Verifier::check_invariant_stability(const Assignment& /*command*/) {
	// TODO: get invariant from somewhere
	throw std::logic_error("not yet implemented (Verifier::check_invariant_stability)");
	// if (!post_maintains_invariant(*current_annotation, <invariant>, command)) {
	// 	std::stringstream msg;
	// 	msg << "Invariant violated by '";
	// 	cola::print(command, msg);
	// 	msg << "'.";
	// 	throw VerificationError(msg.str());
	// }
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

std::unique_ptr<Annotation> get_init_annotation() {
	// TODO: how to initialize annotation??
	return plankton::make_true();
}

void Verifier::visit_interface_function(const Function& function) {
	assert(function.kind == Function::Kind::INTERFACE);
	inside_atomic = false;

	// TODO: get method specification and remember it?
	// TODO: add restrictions on parameters (like -inf<k<inf)? from specification?
	current_annotation = get_init_annotation();
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
	bool old_inside_atomic = inside_atomic;
	inside_atomic = true;
	stmt.body->accept(*this);
	inside_atomic = old_inside_atomic;
	apply_interference();
}

void Verifier::visit(const Choice& stmt) {
	auto pre_condition = std::move(current_annotation);
	std::vector<std::unique_ptr<Annotation>> post_conditions;

	for (const auto& branch : stmt.branches) {
		current_annotation = plankton::copy(*pre_condition);
		branch->accept(*this);
		post_conditions.push_back(std::move(current_annotation));
	}
	
	current_annotation = plankton::unify(post_conditions);
}

void Verifier::visit(const IfThenElse& stmt) {
	auto pre_condition = plankton::copy(*current_annotation);

	// if branch
	Assume dummyIf(cola::copy(*stmt.expr));
	dummyIf.accept(*this);
	stmt.ifBranch->accept(*this);
	auto postIf = std::move(current_annotation);

	// else branch
	current_annotation = std::move(pre_condition);
	Assume dummyElse(cola::negate(*stmt.expr));
	dummyElse.accept(*this);
	stmt.elseBranch->accept(*this);

	current_annotation = plankton::unify(*current_annotation, *postIf);
}

void Verifier::handle_loop(const ConditionalLoop& stmt, bool /*peelFirst*/) { // consider peelFirst a suggestion
	if (dynamic_cast<const BooleanValue*>(stmt.expr.get()) == nullptr || !dynamic_cast<const BooleanValue*>(stmt.expr.get())->value) {
		// TODO: make check semantic --> if (!plankton::implies(current_annotation, stmt.expr)) 
		throw UnsupportedConstructError("while/do-while loop with condition other than 'true'");
	}

	auto outer_breaking_annotations = std::move(breaking_annotations);

	while (true) {
		breaking_annotations.clear();
		auto previous_annotation = plankton::copy(*current_annotation);
		stmt.body->accept(*this);
		if (plankton::is_equal(*previous_annotation, *current_annotation)) {
			break;
		}
		current_annotation = plankton::unify(*current_annotation, *previous_annotation);
	}

	current_annotation = plankton::unify(breaking_annotations);
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

// TODO: whenever a next field is accessed, ensure that the dereference is safe (not null)

void Verifier::visit(const Skip& /*cmd*/) {
	/* do nothing */
}

void Verifier::visit(const Break& /*cmd*/) {
	breaking_annotations.push_back(std::move(current_annotation));
	current_annotation = plankton::make_false();
}

void Verifier::visit(const Continue& /*cmd*/) {
	throw UnsupportedConstructError("'continue'");
}

void Verifier::visit(const Assume& cmd) {
	check_pointer_accesses(*cmd.expr);
	current_annotation = plankton::post_full(std::move(current_annotation), cmd);
	if (has_effect(*cmd.expr)) apply_interference();
}

void Verifier::visit(const Assert& cmd) {
	check_pointer_accesses(*cmd.expr);
	if (!plankton::implies(*current_annotation->now, *cmd.expr)) {
		throw AssertionError(cmd);
	}
}

void Verifier::visit(const Return& cmd) {
	// TODO: extend breaking_annotations?
	for (const auto& expr : cmd.expressions) {
		check_pointer_accesses(*expr);
	}
	returning_annotations.push_back(std::move(current_annotation));
	current_annotation = plankton::make_false();
}

void Verifier::visit(const Malloc& cmd) {
	// TODO: extend interference?
	if (cmd.lhs.is_shared) {
		throw UnsupportedConstructError("allocation targeting shared variable '" + cmd.lhs.name + "'");
	}
	current_annotation = plankton::post_full(std::move(current_annotation), cmd);
}

void Verifier::visit(const Assignment& cmd) {
	check_pointer_accesses(*cmd.lhs);
	check_pointer_accesses(*cmd.rhs);

	// check invariant and extend interference for effectful commands
	if (has_effect(*cmd.lhs)) {
		check_invariant_stability(cmd);
		extend_interference(cmd);
	}

	// compute post annotation
	current_annotation = plankton::post_full(std::move(current_annotation), cmd);
	if (has_effect(*cmd.lhs) || has_effect(*cmd.rhs)) apply_interference();
}

void Verifier::visit_macro_function(const Function& function) {
	auto outer_breaking_annotations = std::move(breaking_annotations);
	auto outer_returning_annotations = std::move(returning_annotations);
	breaking_annotations.clear();
	returning_annotations.clear();
	
	function.body->accept(*this);

	breaking_annotations = std::move(outer_breaking_annotations);
	returning_annotations = std::move(outer_returning_annotations);
}

void Verifier::visit(const Macro& cmd) {
	// pass arguments to function (treated as assignments)
	for (std::size_t i = 0; i < cmd.args.size(); ++i) {
		auto dummy = std::make_unique<Assignment>(
			std::make_unique<VariableExpression>(*cmd.decl.args.at(i)), cola::copy(*cmd.args.at(i))
		);
		assert(!has_effect(*dummy->lhs));
		dummy->accept(*this);
	}

	// descend into function call
	cmd.decl.body->accept(*this);

	// get returned values (treated as assignments)
	for (std::size_t i = 0; i < cmd.lhs.size(); ++i) {
		auto dummy = std::make_unique<Assignment>(
			std::make_unique<VariableExpression>(cmd.lhs.at(i).get()), std::make_unique<VariableExpression>(*cmd.decl.returns.at(i))
		);
		assert(!has_effect(*dummy->lhs));
		dummy->accept(*this);
	}
}

void Verifier::visit(const CompareAndSwap& /*cmd*/) {
	throw UnsupportedConstructError("CompareAndSwap");
}
