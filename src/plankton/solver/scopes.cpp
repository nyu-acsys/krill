#include "plankton/solver/solverimpl.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "cola/util.hpp"
#include "plankton/config.hpp"
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


struct CandidateGenerator {
	const SolverImpl& solver;
	Encoder& encoder;
	const PostConfig& config;
	const VariableDeclaration& decl;
	const std::deque<const VariableDeclaration*>& others;
	std::unique_ptr<ConjunctionFormula> result;

	CandidateGenerator(const SolverImpl& solver, const PostConfig& config, const VariableDeclaration& newVar)
		: solver(solver), encoder(solver.GetEncoder()), config(config), decl(newVar), others(solver.GetVariablesInScope()) {}

	void AddExpressions(const VariableDeclaration& other);
	void AddAxioms(std::unique_ptr<Axiom> axiom);
	void AddAxioms(const Expression& expr, const Expression& other);
	void AddAxioms(const VariableDeclaration& other);

	std::unique_ptr<ConjunctionFormula> MakeCandidates();
	void FilterResult();
};

inline std::vector<BinaryExpression::Operator> MakeOperators(const Expression& expr, const Expression& other) {
	using OP = BinaryExpression::Operator;
	if (expr.sort() != other.sort()) return {};
	if (expr.sort() == Sort::DATA) return { OP::EQ, OP::LT, OP::LEQ, OP::GT, OP::GEQ, OP::NEQ };
	return { OP::EQ, OP::NEQ };
}

inline std::vector<std::unique_ptr<Expression>> MakeExpressions(const VariableDeclaration& decl, bool extend=false) {
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

void CandidateGenerator::AddExpressions(const VariableDeclaration& other) {
	auto declExpressions = MakeExpressions(decl);
	auto otherExpressions = MakeExpressions(other, true);

	for (const auto& declExpr : declExpressions) {
		for (const auto& otherExpr : otherExpressions) {
			if (plankton::syntactically_equal(*declExpr, *otherExpr)) continue;
			for (auto op : MakeOperators(*declExpr, *otherExpr)) {
				result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(
					std::make_unique<BinaryExpression>(op, cola::copy(*declExpr), cola::copy(*otherExpr))
				));
			}
		}
	}
}

void CandidateGenerator::AddAxioms(std::unique_ptr<Axiom> axiom) {
	auto negated = std::make_unique<NegatedAxiom>(plankton::copy(*axiom));
	result->conjuncts.push_back(std::move(axiom));
	result->conjuncts.push_back(std::move(negated));
}

void CandidateGenerator::AddAxioms(const Expression& expr, const Expression& other) {
	// ownership
	if (expr.sort() == Sort::PTR) {
		if (auto varExpr = dynamic_cast<const VariableExpression*>(&expr)) {
			AddAxioms(std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(varExpr->decl)));
		}
	}

	// logically contained
	if (expr.sort() == Sort::DATA) {
		AddAxioms(std::make_unique<LogicallyContainedAxiom>(cola::copy(expr)));
	}

	// keyset contains
	if (expr.sort() == Sort::PTR && other.sort() == Sort::DATA) {
		AddAxioms(std::make_unique<KeysetContainsAxiom>(cola::copy(expr), cola::copy(other)));
	}
	if (other.sort() == Sort::PTR && expr.sort() == Sort::DATA) {
		AddAxioms(std::make_unique<KeysetContainsAxiom>(cola::copy(other), cola::copy(expr)));
	}

	// has flow
	if (expr.sort() == Sort::PTR) {
		AddAxioms(std::make_unique<HasFlowAxiom>(cola::copy(expr)));
	}

	// flow contains
	// TODO: FlowContainsAxiom has three parameters ==> we consider only a fraction of the possibilities here
	if (expr.sort() == Sort::PTR && other.sort() == Sort::DATA) {
		AddAxioms(std::make_unique<FlowContainsAxiom>(cola::copy(expr), cola::copy(other), cola::copy(other)));
	}
	if (other.sort() == Sort::PTR && expr.sort() == Sort::DATA) {
		AddAxioms(std::make_unique<FlowContainsAxiom>(cola::copy(other), cola::copy(expr), cola::copy(expr)));
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
}

void CandidateGenerator::AddAxioms(const VariableDeclaration& other) {
	auto axioms = std::make_unique<AxiomConjunctionFormula>();
	auto declExpressions = MakeExpressions(decl);
	auto otherExpressions = MakeExpressions(other, true);

	for (const auto& declExpr : declExpressions) {
		for (const auto& otherExpr : otherExpressions) {
			AddAxioms(*declExpr, *otherExpr);
		}
	}
}

void CandidateGenerator::FilterResult() {
	auto premise = std::make_unique<ConjunctionFormula>();
	for (auto var : solver.GetVariablesInScope()) {
		if (cola::assignable(config.invariant->vars.at(0)->type, var->type)) {
			premise = plankton::conjoin(std::move(premise), config.invariant->instantiate(*var));
		}
	}

	ImplicationCheckerImpl checker(encoder, *premise);
	auto candidates = std::move(result);
	result = std::make_unique<ConjunctionFormula>();
	for (auto& conjunct : candidates->conjuncts) {
		if (checker.Implies(!encoder.Encode(*conjunct))) continue;
		result->conjuncts.push_back(std::move(conjunct));
	}
}

std::unique_ptr<ConjunctionFormula> CandidateGenerator::MakeCandidates() {
	// init
	result = std::make_unique<ConjunctionFormula>();

	// create all candidates
	for (auto other : others) {
		AddExpressions(*other);
		AddAxioms(*other);
	}

	// filter
	if (plankton::config.filter_candidates_by_invariant) {
		FilterResult();
	}

	// done
	return std::move(result);
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
		// TODO important: ensure that the candidates could generate the invariant
		auto newCandidates = CandidateGenerator(*this, config, *decl).MakeCandidates();
		candidateFormulas.back() = plankton::conjoin(std::move(candidateFormulas.back()), std::move(newCandidates));

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
