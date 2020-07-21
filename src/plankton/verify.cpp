#include "plankton/verify.hpp"

#include <set>
#include <sstream>
#include "cola/util.hpp"
#include "plankton/config.hpp"
#include "plankton/logger.hpp" // TODO:remove

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

	void visit(const Skip& /*node*/) override { /* do nothing */ }
	void visit(const Break& /*node*/) override { /* do nothing */ }
	void visit(const Continue& /*node*/) override { /* do nothing */ }
	void visit(const Malloc& /*node*/) override { /* do nothing */ }
	void visit(const Assume& node) override { node.expr->accept(*this); }
	void visit(const Assert& node) override { node.expr->accept(*this); }
	void visit(const Return& node) override {
		for (const auto& expr : node.expressions) {
			expr->accept(*this);
		}
	}
	void visit(const Assignment& node) override { node.lhs->accept(*this); node.rhs->accept(*this);}
	void visit(const CompareAndSwap& node) override {
		for (const auto& elem : node.elems) {
			elem.dst->accept(*this);
			elem.cmp->accept(*this);
			elem.src->accept(*this);
		}
	}
};

void Verifier::check_pointer_accesses(const Command& command) {
	DereferencedExpressionCollector collector;
	command.accept(collector);

	for (const Expression* expr : collector.exprs) {
		if (!solver->ImpliesNonNull(*current_annotation, *expr)) {
			throw UnsafeDereferenceError(*expr, command);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::visit(const Program& program) {
	solver = plankton::MakeLinearizabilitySolver(program);
	solver->EnterScope(program);

	// TODO: check initializer
	// compute fixed point
	is_interference_saturated = true;
	do {
		for (const auto& func : program.functions) {
			if (func->kind == Function::Kind::INTERFACE) {
				visit_interface_function(*func);
			}
		}
		throw std::logic_error("point du break");
	} while (!is_interference_saturated);

	solver->LeaveScope();
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
	std::unique_ptr<ImplicationChecker> checker;
	SpecStore specification;
	bool result = false;

	FulfillmentSearcher(std::unique_ptr<ImplicationChecker> checker_, SpecStore spec_) : checker(std::move(checker_)), specification(spec_) {}

	void exit(const FulfillmentAxiom& formula) override {
		if (result) return;
		if (formula.kind == specification.kind && &formula.key->decl == &specification.searchKey) {
			auto conclusion = std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
				BinaryExpression::Operator::EQ,
				std::make_unique<VariableExpression>(specification.returnedVar),
				std::make_unique<BooleanValue>(formula.return_value)
			));
			result = checker->Implies(*conclusion);
		}
	}
};

inline void establish_linearizability_or_fail(const Solver& solver, const Annotation& annotation, const Function& function, SpecStore spec) {
	auto checker = solver.MakeImplicationChecker(annotation);

	// check for false
	if (checker->ImpliesFalse()) {
		std::cout << "\% successfully established linearizability (vacuously fulfilled)" << std::endl;
		return;
	}

	// search for fulfillment
	FulfillmentSearcher listener(std::move(checker), spec);
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
	solver->EnterScope(function);

	// handle body
	function.body->accept(*this);
	// std::cout << std::endl << "________" << std::endl << "Post annotation for function '" << function.name << "':" << std::endl;
	// plankton::print(*current_annotation, std::cout);
	// std::cout << std::endl << std::endl;

	// check if obligation is fulfilled
	returning_annotations.push_back(std::move(current_annotation));
	for (const auto& returned : returning_annotations) {
		establish_linearizability_or_fail(*solver, *returned, function, spec);
	}

	current_annotation = Annotation::make_false();
	solver->LeaveScope();
	throw std::logic_error("breakpoint");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void Verifier::visit(const Sequence& stmt) {
	stmt.first->accept(*this);
	stmt.second->accept(*this);
}

void Verifier::visit(const Scope& stmt) {
	// TODO: skip scopes that do not define variables? Maybe if it turns out to boost performance.
	solver->EnterScope(stmt);
	stmt.body->accept(*this);
	solver->LeaveScope();
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
	
	current_annotation = solver->Join(std::move(post_conditions));
	if (plankton::config.interference_after_unification) apply_interference();
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

	current_annotation = solver->Join(std::move(current_annotation), std::move(postIf));
	if (plankton::config.interference_after_unification) apply_interference();
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

		if (solver->Implies(*current_annotation, *before_annotation)) {
			break;
		}
		current_annotation = solver->Join(std::move(before_annotation), std::move(current_annotation));
		if (plankton::config.interference_after_unification) apply_interference();
	}
	
	breaking_annotations.insert(breaking_annotations.begin(),
		std::make_move_iterator(first_breaking_annotations.begin()), std::make_move_iterator(first_breaking_annotations.end())
	);
	current_annotation = solver->Join(std::move(breaking_annotations));

	breaking_annotations = std::move(outer_breaking_annotations);
	returning_annotations.insert(returning_annotations.begin(),
		std::make_move_iterator(outer_returning_annotations.begin()), std::make_move_iterator(outer_returning_annotations.end())
	);

	if (plankton::config.interference_after_unification) apply_interference();
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
	current_annotation = Annotation::make_false();
}

void Verifier::visit(const Continue& /*cmd*/) {
	throw UnsupportedConstructError("'continue'");
}

void Verifier::visit(const Assume& cmd) {
	check_pointer_accesses(cmd);
	current_annotation = solver->Post(*current_annotation, cmd);
	if (has_effect(*cmd.expr)) apply_interference();
}

void Verifier::visit(const Assert& cmd) {
	check_pointer_accesses(cmd);
	if (!solver->Implies(*current_annotation, *cmd.expr)) {
		throw AssertionError(cmd);
	}
}

void Verifier::visit(const Return& cmd) {
	check_pointer_accesses(cmd);
	returning_annotations.push_back(std::move(current_annotation));
	current_annotation = Annotation::make_false();
}

void Verifier::visit(const Malloc& cmd) {
	current_annotation = solver->Post(*current_annotation, cmd);
}

void Verifier::visit(const Assignment& cmd) {
	check_pointer_accesses(cmd);

	// check invariant and extend interference for effectful commands
	if (has_effect(*cmd.lhs)) {
		extend_interference(cmd);
	}

	// compute post annotation
	current_annotation = solver->Post(*current_annotation, cmd);
	if (has_effect(*cmd.lhs) || has_effect(*cmd.rhs)) apply_interference();
}

void Verifier::visit_macro_function(const Function& function) {
	auto outer_breaking_annotations = std::move(breaking_annotations);
	auto outer_returning_annotations = std::move(returning_annotations);
	breaking_annotations.clear();
	returning_annotations.clear();
	
	function.body->accept(*this);
	returning_annotations.push_back(std::move(current_annotation));
	current_annotation = solver->Join(std::move(returning_annotations));

	breaking_annotations = std::move(outer_breaking_annotations);
	returning_annotations = std::move(outer_returning_annotations);
}


template<typename T>
inline std::pair<const T*, std::unique_ptr<T>> prepare(const VariableDeclaration& decl) {
	std::pair<const T*, std::unique_ptr<T>> result;
	result.second = std::make_unique<VariableExpression>(decl);
	result.first = result.second.get();
	return result;
}

template<typename T>
inline std::pair<const T*, std::unique_ptr<T>> prepare(const std::unique_ptr<VariableDeclaration>& decl) {
	std::pair<const T*, std::unique_ptr<T>> result;
	result.second = std::make_unique<VariableExpression>(*decl);
	result.first = result.second.get();
	return result;
}

template<typename T>
inline std::pair<const T*, std::unique_ptr<T>> prepare(const std::unique_ptr<T>& expr) {
	std::pair<const T*, std::unique_ptr<T>> result;
	result.first = expr.get();
	return result;
}

template<typename U, typename V>
inline std::unique_ptr<Annotation> execute_parallel_assignment(const Solver& solver, const Annotation& pre, const std::vector<U>& lhs, const std::vector<V>& rhs) {
	assert(lhs.size() == rhs.size());

	std::deque<std::unique_ptr<Expression>> dummy_storage;
	plankton::Solver::parallel_assignment_t assignment;

	for (std::size_t index = 0; index < lhs.size(); ++index) {
		auto [leftExpr, leftOwn] = prepare<VariableExpression>(lhs.at(index));
		auto [rightExpr, rightOwn] = prepare<Expression>(rhs.at(index));
		if (leftOwn) dummy_storage.push_back(std::move(leftOwn));
		if (rightOwn) dummy_storage.push_back(std::move(rightOwn));
		assignment.push_back(std::make_pair(std::cref(*leftExpr), std::cref(*rightExpr)));
	}

	return solver.Post(pre, assignment);
}

void Verifier::visit(const Macro& cmd) {
	// pass arguments
	current_annotation = solver->AddInvariant(std::move(current_annotation));
	solver->EnterScope(cmd);
	current_annotation = execute_parallel_assignment(*solver, *current_annotation, cmd.decl.args, cmd.args);

	// descend into function call
	visit_macro_function(cmd.decl);

	// grab returns
	current_annotation = solver->AddInvariant(std::move(current_annotation));
	solver->LeaveScope();
	current_annotation = execute_parallel_assignment(*solver, *current_annotation, cmd.lhs, cmd.decl.returns);

	// log() << std::endl << "________" << std::endl << "Post annotation for macro '" << cmd.decl.name << "':" << std::endl << *current_annotation << std::endl << std::endl;
}

void Verifier::visit(const CompareAndSwap& /*cmd*/) {
	throw UnsupportedConstructError("CompareAndSwap");
}
