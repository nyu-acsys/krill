#include "plankton/solver/solverimpl.hpp"

#include <iostream> // TODO: delete
#include "cola/util.hpp"
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


std::vector<BinaryExpression::Operator> make_operators(const Expression& expr, const Expression& other) {
	using OP = BinaryExpression::Operator;
	if (expr.sort() != other.sort()) return {};
	if (expr.sort() == Sort::DATA) return { OP::EQ, OP::LT, OP::LEQ, OP::GT, OP::GEQ, OP::NEQ };
	return { OP::EQ, OP::NEQ };
}

std::vector<std::unique_ptr<Expression>> make_expressions(const VariableDeclaration& decl, bool extend=false) {
	std::vector<std::unique_ptr<Expression>> result;
	result.reserve(decl.type.fields.size() + 6);
	result.push_back(std::make_unique<VariableExpression>(decl));
	for (auto [declField, declFieldType] : decl.type.fields) {
		result.push_back(std::make_unique<Dereference>(std::make_unique<VariableExpression>(decl), declField));
	}
	if (extend) {
		result.push_back(std::make_unique<NullValue>());
		result.push_back(std::make_unique<BooleanValue>(true));
		result.push_back(std::make_unique<BooleanValue>(false));
		result.push_back(std::make_unique<MaxValue>());
		result.push_back(std::make_unique<MinValue>());
	}
	return result;
}

std::unique_ptr<ConjunctionFormula> make_candidate_expressions(const VariableDeclaration& decl, const VariableDeclaration& other) {
	auto result = std::make_unique<ConjunctionFormula>();
	auto declExpressions = make_expressions(decl);
	auto otherExpressions = make_expressions(other, true);

	for (const auto& declExpr : declExpressions) {
		for (const auto& otherExpr : otherExpressions) {
			if (plankton::syntactically_equal(*declExpr, *otherExpr)) continue;
			for (auto op : make_operators(*declExpr, *otherExpr)) {
				result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(
					std::make_unique<BinaryExpression>(op, cola::copy(*declExpr), cola::copy(*otherExpr))
				));
			}
		}
	}

	return result;
}

std::unique_ptr<AxiomConjunctionFormula> make_candidate_axioms(const Expression& expr, const Expression& other) {
	auto result = std::make_unique<AxiomConjunctionFormula>();

	// ownership
	if (expr.sort() == Sort::PTR) {
		if (auto varExpr = dynamic_cast<const VariableExpression*>(&expr)) {
			result->conjuncts.push_back(std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(varExpr->decl)));
		}
	}

	// logically contained
	if (expr.sort() == Sort::DATA) {
		result->conjuncts.push_back(std::make_unique<LogicallyContainedAxiom>(cola::copy(expr)));
	}

	// keyset contains
	if (expr.sort() == Sort::PTR && other.sort() == Sort::DATA) {
		result->conjuncts.push_back(std::make_unique<KeysetContainsAxiom>(cola::copy(expr), cola::copy(other)));
	}
	if (other.sort() == Sort::PTR && expr.sort() == Sort::DATA) {
		result->conjuncts.push_back(std::make_unique<KeysetContainsAxiom>(cola::copy(other), cola::copy(expr)));
	}

	// has flow
	if (expr.sort() == Sort::PTR) {
		result->conjuncts.push_back(std::make_unique<HasFlowAxiom>(cola::copy(expr)));
	}

	// flow contains
	// TODO: FlowContainsAxiom has three parameters ==> we consider only a fraction of the possibilities here
	if (expr.sort() == Sort::PTR && other.sort() == Sort::DATA) {
		result->conjuncts.push_back(std::make_unique<FlowContainsAxiom>(cola::copy(expr), cola::copy(other), cola::copy(other)));
	}
	if (other.sort() == Sort::PTR && expr.sort() == Sort::DATA) {
		result->conjuncts.push_back(std::make_unique<FlowContainsAxiom>(cola::copy(other), cola::copy(expr), cola::copy(expr)));
	}

	// obligations & fulfillments
	if (expr.sort() == Sort::DATA) {
		if (auto varExpr = dynamic_cast<const VariableExpression*>(&expr)) {
			for (auto kind : SpecificationAxiom::KindIteratable) {
				result->conjuncts.push_back(std::make_unique<ObligationAxiom>(kind, std::make_unique<VariableExpression>(varExpr->decl)));
				result->conjuncts.push_back(std::make_unique<FulfillmentAxiom>(kind, std::make_unique<VariableExpression>(varExpr->decl), true));
				result->conjuncts.push_back(std::make_unique<FulfillmentAxiom>(kind, std::make_unique<VariableExpression>(varExpr->decl), false));
			}
		}
	}

	return result;
}

std::unique_ptr<ConjunctionFormula> make_candidate_axioms(const VariableDeclaration& decl, const VariableDeclaration& other) {
	auto axioms = std::make_unique<AxiomConjunctionFormula>();
	auto declExpressions = make_expressions(decl);
	auto otherExpressions = make_expressions(other, true);

	for (const auto& declExpr : declExpressions) {
		for (const auto& otherExpr : otherExpressions) {
			auto candidates = make_candidate_axioms(*declExpr, *otherExpr);
			axioms = plankton::conjoin(std::move(axioms), std::move(candidates));
		}
	}

	auto result = std::make_unique<ConjunctionFormula>();
	for (auto& axiom : axioms->conjuncts) {
		auto negated = std::make_unique<NegatedAxiom>(plankton::copy(*axiom));
		result->conjuncts.push_back(std::move(axiom));
		result->conjuncts.push_back(std::move(negated));
	}

	return result;
}

std::unique_ptr<ConjunctionFormula> make_candidates(const VariableDeclaration& decl, const VariableDeclaration& other) {
	auto exprs = make_candidate_expressions(decl, other);
	auto axioms = make_candidate_axioms(decl, other);
	return plankton::conjoin(std::move(exprs), std::move(axioms));
}

std::unique_ptr<ConjunctionFormula> make_candidates(const VariableDeclaration& decl, const std::deque<const VariableDeclaration*>& others) {
	auto result = std::make_unique<ConjunctionFormula>();
	for (auto other : others) {
		auto candidates = make_candidates(decl, *other);
		result = plankton::conjoin(std::move(result), std::move(candidates));
	}

	// std::cout << "Candidates: " << std::endl;
	// plankton::print(*result, std::cout);
	// std::cout << std::endl << std::endl;
	return result;
}


static constexpr std::string_view ERR_ENTER_PROGRAM = "Cannot enter program scope: program scope must be the outermost scope.";
static constexpr std::string_view ERR_ENTER_NOSCOPE = "Cannot enter scope: there must be an outermost program scope added first.";
static constexpr std::string_view ERR_LEAVE_NOSCOPE = "Cannot leave scope: there is no scope left to leave.";

inline void fail_if(bool fail, const std::string_view msg) {
	if (fail) {
		throw SolvingError(std::string(msg));
	}
}

void SolverImpl::ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars) {
	assert(!instantiatedInvariants.empty());
	assert(!candidateFormulas.empty());
	assert(!variablesInScope.empty());

	for (const auto& decl : vars) {
		variablesInScope.back().push_back(decl.get());
		auto new_candidates = make_candidates(*decl, variablesInScope.back());
		candidateFormulas.back() = plankton::conjoin(std::move(candidateFormulas.back()), std::move(new_candidates));

		if (cola::assignable(config.invariant->vars.at(0)->type, decl->type)) {
			instantiatedInvariants.back() = plankton::conjoin(std::move(instantiatedInvariants.back()), config.invariant->instantiate(*decl));
		}
	}
}

void SolverImpl::EnterScope(const cola::Program& program) {
	fail_if(!candidateFormulas.empty(), ERR_ENTER_PROGRAM);
	candidateFormulas.push_back(std::make_unique<ConjunctionFormula>());
	instantiatedInvariants.push_back(std::make_unique<ConjunctionFormula>());
	variablesInScope.emplace_back();

	ExtendCurrentScope(program.variables);
}

void SolverImpl::PushOuterScope() {
	// copy from outermost scope (front)
	fail_if(candidateFormulas.empty(), ERR_ENTER_NOSCOPE);
	candidateFormulas.push_back(plankton::copy(*candidateFormulas.front()));
	instantiatedInvariants.push_back(plankton::copy(*instantiatedInvariants.front()));
	variablesInScope.push_back(std::deque<const VariableDeclaration*>(variablesInScope.front()));
}

void SolverImpl::PushInnerScope() {
	// copy from innermost scope (back)
	fail_if(candidateFormulas.empty(), ERR_ENTER_NOSCOPE);
	candidateFormulas.push_back(plankton::copy(*candidateFormulas.back()));
	instantiatedInvariants.push_back(plankton::copy(*instantiatedInvariants.back()));
	variablesInScope.push_back(std::deque<const VariableDeclaration*>(variablesInScope.back()));
}

void SolverImpl::EnterScope(const cola::Function& function) {
	PushOuterScope();
	ExtendCurrentScope(function.args);
	ExtendCurrentScope(function.returns);
}

void SolverImpl::EnterScope(const cola::Scope& scope) {
	PushInnerScope();
	ExtendCurrentScope(scope.variables);
}

void SolverImpl::LeaveScope() {
	fail_if(candidateFormulas.empty(), ERR_LEAVE_NOSCOPE);
	instantiatedInvariants.pop_back();
	candidateFormulas.pop_back();
	variablesInScope.pop_back();
}
