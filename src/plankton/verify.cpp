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
		replacement = newDecl.get();
		variable2renamed[&decl] = replacement;
		variable2renamed[replacement] = replacement; // prevent renamed variable from being renamed // TODO: why?
		renamed_variables.push_back(std::move(newDecl));
	}

	return *replacement;
}

transformer_t RenamingInfo::as_transformer() {
	return [this](const auto& expr){
		std::unique_ptr<Expression> replacement;
		auto check = is_of_type<VariableExpression>(expr);
		if (check.first) {
			replacement = std::make_unique<VariableExpression>(this->rename(check.second->decl));
		}
		return std::make_pair(replacement ? true : false, std::move(replacement));
	};
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct DereferencedExpressionCollector : BaseVisitor {
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
	DereferencedExpressionCollector collector;
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

void throw_invariant_violation_if(bool flag, const Command& command, std::string more_message="") {
	if (flag) {
		std::stringstream msg;
		msg << "Invariant violated" << more_message << ", at '";
		cola::print(command, msg);
		msg << "'.";
		throw VerificationError(msg.str());
	}
}

void Verifier::check_invariant_stability(const Assignment& command) {
	throw_invariant_violation_if(!post_maintains_invariant(*current_annotation, command), command);
}

void Verifier::check_invariant_stability(const Malloc& command) {
	if (command.lhs.is_shared) {
		throw UnsupportedConstructError("allocation targeting shared variable '" + command.lhs.name + "'");
	}
	throw_invariant_violation_if(!plankton::post_maintains_invariant(*current_annotation, command), command, " by newly allocated node");
}

void Verifier::push_invariant_instantiation(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars) {
	const auto& invariant = plankton::config->get_invariant();
	const auto& argtype = invariant.vars.at(0)->type;
	
	for (const auto& var : vars) {
		if (cola::assignable(argtype, var->type)) {
			instantiated_invariant = plankton::conjoin(std::move(instantiated_invariant), invariant.instantiate(*var));
		}	
	}
}

void Verifier::exploint_invariant() { // TODO: delete parameter
	assert(instantiated_invariant);
	current_annotation->now = plankton::conjoin(std::move(current_annotation->now), plankton::copy(*instantiated_invariant));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::visit(const Program& program) {
	set_config(program);

	// TODO: check initializer
	// compute fixed point
	is_interference_saturated = true;
	do {
		for (const auto& func : program.functions) {
			if (func->kind == Function::Kind::INTERFACE) {
				instantiated_invariant = std::make_unique<ConjunctionFormula>();
				push_invariant_instantiation(program.variables);

				visit_interface_function(*func);
			}
		}
		throw std::logic_error("point du break");
	} while (!is_interference_saturated);
}

struct SpecStore {
	SpecificationAxiom::Kind kind;
	const VariableDeclaration& searchKey;
	const VariableDeclaration& returnedVar;
};

SpecStore get_spec(const Function& function) {
	SpecificationAxiom::Kind kind;
	if (function.name == "contains") {
		kind =  SpecificationAxiom::Kind::CONTAINS;
	} else if (function.name == "insert" || function.name == "add") {
		kind =  SpecificationAxiom::Kind::INSERT;
	} else if (function.name == "delete" || function.name == "remove") {
		kind =  SpecificationAxiom::Kind::DELETE;
	} else {
		throw UnsupportedConstructError("Specification for function '" + function.name + "' unknown, expected one of: 'contains', 'insert', 'add', 'delete', 'remove'.");
	}
	if (function.args.size() != 1) {
		throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected 1 parameter, got " + std::to_string(function.args.size()) + ".");
	}
	if (function.returns.size() != 1) {
		throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected 1 return value, got " + std::to_string(function.returns.size()) + ".");
	}
	if (&function.returns.at(0)->type != &Type::bool_type()) {
		throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected boolean return value.");
	}
	return { kind, *function.args.at(0), *function.returns.at(0) };
}

std::unique_ptr<Annotation> get_init_annotation(SpecStore spec) {
	auto result = Annotation::make_true();
	result->now->conjuncts.push_back(
		std::make_unique<ObligationAxiom>(spec.kind, std::make_unique<VariableExpression>(spec.searchKey)) // obligation(specKind, searchKey)
	);
	// TODO: have those restrictions come from the program?
	result->now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::LT, std::make_unique<MinValue>(), std::make_unique<VariableExpression>(spec.searchKey) // -∞ < searchKey
	)));
	result->now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::LT, std::make_unique<VariableExpression>(spec.searchKey), std::make_unique<MaxValue>() // searchKey < ∞
	)));
	return result;
}

struct FulfillmentSearcher : public DefaultListener {
	const Annotation& annotation;
	SpecStore spec;
	bool result = false;

	FulfillmentSearcher(const Annotation& annotation, SpecStore spec) : annotation(annotation), spec(spec) {}

	void exit(const FulfillmentAxiom& formula) override {
		if (result) return;
		if (formula.kind == spec.kind && &formula.key->decl == &spec.searchKey) {
			auto conclusion = std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
				BinaryExpression::Operator::EQ,
				std::make_unique<VariableExpression>(spec.returnedVar),
				std::make_unique<BooleanValue>(formula.return_value)
			));
			result = plankton::implies(*annotation.now, *conclusion);
		}
	}
};

void establish_linearizability_or_fail(const Annotation& annotation, const Function& function, SpecStore spec) {
	// check for false
	static ExpressionAxiom falseFormula(std::make_unique<BooleanValue>(false));
	if (plankton::implies(*annotation.now, falseFormula)) {
		std::cout << "\% successfully established linearizability (vacuously fulfilled)" << std::endl;
		return;
	}

	// search for fulfillment
	FulfillmentSearcher listener(annotation, spec);
	annotation.now->accept(listener);
	if (listener.result) {
		std::cout << "\% successfully established linearizability (found fulfillment)" << std::endl;
		return;
	}

	std::cout << "\% could not establish linearizability" << std::endl;
	// throw VerificationError("Could not establish linearizability for function '" + function.name + "'.");
}

void Verifier::visit_interface_function(const Function& function) {
	std::cout << std::endl << std::endl << std::endl << std::endl << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << "#################### " << function.name << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << std::endl;

	assert(function.kind == Function::Kind::INTERFACE);
	inside_atomic = false;

	// extract specification info and initial annotation
	auto spec = get_spec(function);
	current_annotation = get_init_annotation(spec);

	// handle body
	function.body->accept(*this);
	std::cout << std::endl << "________" << std::endl << "Post annotation for function '" << function.name << "':" << std::endl;
	plankton::print(*current_annotation, std::cout);
	std::cout << std::endl << std::endl;

	// check if obligation is fulfilled
	establish_linearizability_or_fail(*current_annotation, function, spec);
	for (const auto& returned : returning_annotations) {
		establish_linearizability_or_fail(*returned, function, spec);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::visit(const Sequence& stmt) {
	stmt.first->accept(*this);
	stmt.second->accept(*this);
}

void Verifier::visit(const Scope& stmt) {
	push_invariant_instantiation(stmt.variables);
	stmt.body->accept(*this);
	// TODO: should we reset 'instantiated_invariant' when variables go out of scope?
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
	
	current_annotation = plankton::unify(std::move(post_conditions));
	if (plankton::config->interference_after_unification) apply_interference();
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

	current_annotation = plankton::unify(std::move(current_annotation), std::move(postIf));
	if (plankton::config->interference_after_unification) apply_interference();
}

void Verifier::handle_loop(const ConditionalLoop& stmt, bool /*peelFirst*/) { // consider peelFirst a suggestion
	if (dynamic_cast<const BooleanValue*>(stmt.expr.get()) == nullptr || !dynamic_cast<const BooleanValue*>(stmt.expr.get())->value) {
		// TODO: make check semantic --> if (!plankton::implies(current_annotation, stmt.expr)) 
		throw UnsupportedConstructError("while/do-while loop with condition other than 'true'");
	}

	auto outer_breaking_annotations = std::move(breaking_annotations);
	breaking_annotations.clear();
	std::size_t counter = 0;

	// peel first loop iteration
	std::cout << std::endl << std::endl << " ------ loop " << counter++ << " (peeled) ------ " << std::endl;
	stmt.body->accept(*this);
	auto first_breaking_annotations = std::move(breaking_annotations);
	auto outer_returning_annotations = std::move(returning_annotations);

	while (true) {
		std::cout << std::endl << std::endl << " ------ loop " << counter++ << " ------ " << std::endl;

		breaking_annotations.clear();
		returning_annotations.clear();

		auto before_annotation = plankton::copy(*current_annotation);
		stmt.body->accept(*this);

		if (plankton::implies(*current_annotation, *before_annotation)) {
			break;
		}
		current_annotation = plankton::unify(std::move(before_annotation), std::move(current_annotation));
		if (plankton::config->interference_after_unification) apply_interference();
	}
	
	breaking_annotations.insert(breaking_annotations.begin(),
		std::make_move_iterator(first_breaking_annotations.begin()), std::make_move_iterator(first_breaking_annotations.end())
	);
	current_annotation = plankton::unify(std::move(breaking_annotations));

	breaking_annotations = std::move(outer_breaking_annotations);
	returning_annotations.insert(returning_annotations.begin(),
		std::make_move_iterator(outer_returning_annotations.begin()), std::make_move_iterator(outer_returning_annotations.end())
	);

	if (plankton::config->interference_after_unification) apply_interference();
}

// void Verifier::handle_loop(const ConditionalLoop& stmt, bool /*peelFirst*/) { // consider peelFirst a suggestion
// 	if (dynamic_cast<const BooleanValue*>(stmt.expr.get()) == nullptr || !dynamic_cast<const BooleanValue*>(stmt.expr.get())->value) {
// 		// TODO: make check semantic --> if (!plankton::implies(current_annotation, stmt.expr)) 
// 		throw UnsupportedConstructError("while/do-while loop with condition other than 'true'");
// 	}

// 	auto outer_breaking_annotations = std::move(breaking_annotations);
// 	std::size_t counter = 0;

// 	while (true) {
// 		std::cout << std::endl << std::endl << " ------ loop " << counter++ << " ------ " << std::endl;

// 		breaking_annotations.clear();
// 		auto before_annotation = plankton::copy(*current_annotation);
// 		stmt.body->accept(*this);

// 		if (plankton::implies(*current_annotation, *before_annotation)) {
// 			break;
// 		}
// 		current_annotation = plankton::unify(std::move(before_annotation), std::move(current_annotation));
// 		apply_interference(); // TODO important: needed?
// 	}

// 	current_annotation = plankton::unify(std::move(breaking_annotations));
// 	breaking_annotations = std::move(outer_breaking_annotations);
// 	apply_interference(); // TODO important: needed?
// }

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
	current_annotation = Annotation::make_false();
}

void Verifier::visit(const Continue& /*cmd*/) {
	throw UnsupportedConstructError("'continue'");
}

void Verifier::visit(const Assume& cmd) {
	exploint_invariant();
	check_pointer_accesses(*cmd.expr);
	current_annotation = plankton::post_full(std::move(current_annotation), cmd);
	exploint_invariant();
	if (has_effect(*cmd.expr)) apply_interference();
}

void Verifier::visit(const Assert& cmd) {
	exploint_invariant();
	check_pointer_accesses(*cmd.expr);
	if (!plankton::implies(*current_annotation->now, *cmd.expr)) {
		throw AssertionError(cmd);
	}
}

void Verifier::visit(const Return& cmd) {
	// TODO: extend breaking_annotations?
	for (const auto& expr : cmd.expressions) {
		exploint_invariant();
		check_pointer_accesses(*expr);
	}
	returning_annotations.push_back(std::move(current_annotation));
	current_annotation = Annotation::make_false();
}

void Verifier::visit(const Malloc& cmd) {
	exploint_invariant();
	check_invariant_stability(cmd);
	current_annotation = plankton::post_full(std::move(current_annotation), cmd);
}

void Verifier::visit(const Assignment& cmd) {
	exploint_invariant();
	check_pointer_accesses(*cmd.lhs);
	check_pointer_accesses(*cmd.rhs);

	// check invariant and extend interference for effectful commands
	if (has_effect(*cmd.lhs)) {
		check_invariant_stability(cmd);
		extend_interference(cmd);
	}

	// compute post annotation
	current_annotation = plankton::post_full(std::move(current_annotation), cmd);
	exploint_invariant();
	if (has_effect(*cmd.lhs) || has_effect(*cmd.rhs)) apply_interference();
}

void Verifier::visit_macro_function(const Function& function) {
	auto outer_breaking_annotations = std::move(breaking_annotations);
	auto outer_returning_annotations = std::move(returning_annotations);
	breaking_annotations.clear();
	returning_annotations.clear();
	
	function.body->accept(*this);
	returning_annotations.push_back(std::move(current_annotation));
	current_annotation = plankton::unify(std::move(returning_annotations));

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
