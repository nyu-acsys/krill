#include "plankton/verify.hpp"

#include <set>
#include <sstream>
#include "cola/util.hpp"
#include "plankton/config.hpp"
#include "plankton/logger.hpp" // TODO:remove

using namespace cola;
using namespace heal;
using namespace plankton;


void ThrowMissingReturn(const Function& function) {
	throw VerificationError("Detected non-returning path through non-void function '" + function.name + "'.");
}

bool is_void(const Function& function) {
	return function.return_type.size() == 1 && &function.return_type.at(0).get() == &Type::void_type();
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
};

SpecStore get_spec(const Function& function) {
	assert(function.kind == Function::INTERFACE);
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
	if (function.return_type.size() != 1) {
		throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected 1 return value, got " + std::to_string(function.return_type.size()) + ".");
	}
	if (&function.return_type.at(0).get() != &Type::bool_type()) {
		throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected return type to be 'bool'.");
	}
	return { kind, *function.args.at(0) };
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

//struct FulfillmentSearcher : public DefaultListener {
//	std::unique_ptr<ImplicationChecker> checker;
//	SpecStore specification;
//	const Return& cmd;
//	bool result = false;
//
//	FulfillmentSearcher(std::unique_ptr<ImplicationChecker> checker_, SpecStore spec_, const Return& cmd_)
//		: checker(std::move(checker_)), specification(spec_), cmd(cmd_) { assert(cmd.expressions.size() == 1); }
//
//	void exit(const FulfillmentAxiom& formula) override {
//		if (result) return;
//		if (formula.kind == specification.kind && &formula.key->decl == &specification.searchKey) {
//			auto conclusion = std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
//				BinaryExpression::Operator::EQ,
//				cola::copy(*cmd.expressions.at(0)),
//				std::make_unique<BooleanValue>(formula.return_value)
//			));
//			result = checker->Implies(*conclusion);
//		}
//	}
//};

inline void establish_linearizability_or_fail(const Solver& /*solver*/, SpecStore /*spec*/, const Annotation& /*annotation*/, const Return* /*cmd*/, const Function& /*function*/) {
	throw std::logic_error("not yet implemented: Verifier::establish_linearizability_or_fail");
//	auto checker = solver.MakeImplicationChecker(annotation);
//
//	// check for false
//	if (checker->ImpliesFalse()) {
//		std::cout << "\% successfully established linearizability (vacuously fulfilled on unreachable path)" << std::endl;
//		return;
//	}
//
//	// check for missing return
//	assert(!is_void(function));
//	if (!cmd) ThrowMissingReturn(function);
//
//	// search for fulfillment
//	FulfillmentSearcher listener(std::move(checker), spec, *cmd);
//	annotation.now->accept(listener);
//	if (listener.result) {
//		std::cout << "\% successfully established linearizability (found fulfillment)" << std::endl;
//		return;
//	}
//
//	std::cout << "\% could not establish linearizability" << std::endl;
//	// throw VerificationError("Could not establish linearizability for function '" + function.name + "'.");
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
	breaking_annotations.clear();
	returning_annotations.clear();

	// extract specification info and initial annotation
	auto spec = get_spec(function);
	current_annotation = get_init_annotation(spec);

	// handle body
	solver->EnterScope(function);
	function.body->accept(*this);
	solver->LeaveScope();

	// check if obligation is fulfilled
	returning_annotations.emplace_back(std::move(current_annotation), nullptr);
	for (const auto& [pre, cmd] : returning_annotations) {
		establish_linearizability_or_fail(*solver, spec, *pre, cmd, function);
	}

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
		current_annotation = heal::copy(*pre_condition);
		branch->accept(*this);
		post_conditions.push_back(std::move(current_annotation));
	}
	
	current_annotation = solver->Join(std::move(post_conditions));
	if (plankton::config.interference_after_unification) apply_interference();
}

void Verifier::visit(const IfThenElse& stmt) {
	// TODO: add PostAssume for expressions to solver to avoid dummy 'Assume's here
	auto pre_condition = heal::copy(*current_annotation);

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

		auto before_annotation = heal::copy(*current_annotation);
		stmt.body->accept(*this);

		if (solver->Implies(*current_annotation, *before_annotation)) break;
		current_annotation = solver->Join(std::move(before_annotation), std::move(current_annotation));
		// current_annotation = solver->Join(plankton::copy(*before_annotation), std::move(current_annotation));
		// if (solver->Implies(*current_annotation, *before_annotation)) {
		// 	break;
		// }

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
	current_annotation = solver->AddInvariant(std::move(current_annotation));
	returning_annotations.emplace_back(std::move(current_annotation), &cmd);
	current_annotation = Annotation::make_false();
}

void Verifier::visit(const Malloc& cmd) {
	current_annotation = solver->Post(*current_annotation, cmd);
}

void Verifier::visit(const Assignment& cmd) {
	check_pointer_accesses(cmd);

	// check invariant and extend interference for effectful commands
	if (has_effect(*cmd.lhs)) {
		// TODO: skip if cmd is an effect on owned memory?
		extend_interference(cmd);
	}

	// compute post annotation
	current_annotation = solver->Post(*current_annotation, cmd);
	if (has_effect(*cmd.lhs) || has_effect(*cmd.rhs)) apply_interference();
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

std::unique_ptr<Annotation> join_returning_annotations(const Solver& solver, const Macro& macro, std::vector<std::pair<std::unique_ptr<Annotation>, const cola::Return*>> returning_annotations) {
	std::vector<std::unique_ptr<Annotation>> postAnnotations;
	for (auto& [returnPre, returnCmd] : returning_annotations) {
		std::unique_ptr<Annotation> post;

		if (returnCmd) // path ends in return command
			post = execute_parallel_assignment(solver, *returnPre, macro.lhs, returnCmd->expressions);
		
		else if (is_void(macro.decl)) // path through void function without return command
			post = std::move(returnPre);

		else if (solver.ImpliesFalse(*returnPre)) // unreachable
			continue;
		
		else // non-returning path through non-void function
			ThrowMissingReturn(macro.decl);

		postAnnotations.push_back(std::move(post));
		// log() << std::endl <<  "------" << std::endl << "return from " << macro.decl.name << " with: " << std::endl << *postAnnotations.back() << std::endl;
	}
	return solver.Join(std::move(postAnnotations));
}

void Verifier::visit(const Macro& cmd) {
	// store caller context
	auto outer_breaking_annotations = std::move(breaking_annotations);
	auto outer_returning_annotations = std::move(returning_annotations);
	breaking_annotations.clear();
	returning_annotations.clear();

	// pass arguments (must be done in inner scope)
	current_annotation = solver->AddInvariant(std::move(current_annotation));
	solver->EnterScope(cmd);
	current_annotation = execute_parallel_assignment(*solver, *current_annotation, cmd.decl.args, cmd.args);

	// descend into function call
	cmd.decl.body->accept(*this);
	current_annotation = solver->AddInvariant(std::move(current_annotation));
	returning_annotations.emplace_back(std::move(current_annotation), nullptr);

	// make post annotation (must be done in outer scope)
	solver->LeaveScope();
	current_annotation = join_returning_annotations(*solver, cmd, std::move(returning_annotations));

	// restore caller context
	breaking_annotations = std::move(outer_breaking_annotations);
	returning_annotations = std::move(outer_returning_annotations);

	log() << std::endl << "________" << std::endl << "Post annotation for macro '" << cmd.decl.name << "':" << std::endl << *current_annotation << std::endl << std::endl;
}

void Verifier::visit(const CompareAndSwap& /*cmd*/) {
	throw UnsupportedConstructError("CompareAndSwap");
}
