#include "plankton/solver/solverimpl.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "cola/util.hpp"
#include "plankton/config.hpp"
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


inline std::unique_ptr<NullValue> MakeNull() { return std::make_unique<NullValue>(); }
inline std::unique_ptr<BooleanValue> MakeBool(bool val) { return std::make_unique<BooleanValue>(val); }
inline std::unique_ptr<MaxValue> MakeMax() { return std::make_unique<MaxValue>(); }
inline std::unique_ptr<MinValue> MakeMin() { return std::make_unique<MinValue>(); }

inline std::unique_ptr<VariableExpression> MakeExpr(const VariableDeclaration& decl) { return std::make_unique<VariableExpression>(decl); }
inline std::unique_ptr<BinaryExpression> MakeExpr(BinaryExpression::Operator op, std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> rhs) { return std::make_unique<BinaryExpression>(op, std::move(lhs), std::move(rhs)); }
inline std::unique_ptr<Dereference> MakeDeref(const VariableDeclaration& decl, std::string field) { return std::make_unique<Dereference>(MakeExpr(decl), field); }

inline std::unique_ptr<ExpressionAxiom> MakeAxiom(std::unique_ptr<Expression> expr) { return std::make_unique<ExpressionAxiom>(std::move(expr)); }
inline std::unique_ptr<OwnershipAxiom> MakeOwnership(const VariableDeclaration& decl) { return std::make_unique<OwnershipAxiom>(MakeExpr(decl)); }
inline std::unique_ptr<NegatedAxiom> MakeNot(std::unique_ptr<Axiom> axiom) { return std::make_unique<NegatedAxiom>(std::move(axiom)); }
inline std::unique_ptr<ObligationAxiom> MakeObligation(SpecificationAxiom::Kind kind, const VariableDeclaration& decl) { return std::make_unique<ObligationAxiom>(kind, MakeExpr(decl)); }
inline std::unique_ptr<FulfillmentAxiom> MakeFulfillment(SpecificationAxiom::Kind kind, const VariableDeclaration& decl, bool returnValue) { return std::make_unique<FulfillmentAxiom>(kind, MakeExpr(decl), returnValue); }
inline std::unique_ptr<LogicallyContainedAxiom> MakeLogicallyContained(std::unique_ptr<Expression> expr) { return std::make_unique<LogicallyContainedAxiom>(std::move(expr)); }
inline std::unique_ptr<HasFlowAxiom> MakeHasFlow(std::unique_ptr<Expression> expr) { return std::make_unique<HasFlowAxiom>(std::move(expr)); }
inline std::unique_ptr<KeysetContainsAxiom> MakeKeysetContains(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) { return std::make_unique<KeysetContainsAxiom>(std::move(expr), std::move(other)); }
inline std::unique_ptr<FlowContainsAxiom> MakeFlowContains(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other, std::unique_ptr<Expression> more) { return std::make_unique<FlowContainsAxiom>(std::move(expr), std::move(other), std::move(more)); }


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<Expression>> MakeAllExpressions(const VariableDeclaration& decl) {
	std::vector<std::unique_ptr<Expression>> result;
	result.reserve(decl.type.fields.size() + 1);

	result.push_back(MakeExpr(decl));
	for (auto [declField, declFieldType] : decl.type.fields) {
		result.push_back(MakeDeref(decl, declField));
	}
	return result;
}

struct ExpressionStore {
	// all unordered distinct pairs of expressions
	std::deque<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> expressions;
	bool skipNull;

	void AddImmediate(const VariableDeclaration& decl);
	void AddCombination(const VariableDeclaration& decl, const VariableDeclaration& other);
	void AddAllCombinations();
	
	ExpressionStore(const SolverImpl& solver, bool skipNull=false) : skipNull(skipNull) {
		auto begin = solver.GetVariablesInScope().begin();
		auto end = solver.GetVariablesInScope().end();
		for (auto iter = begin; iter != end; ++iter) {
			AddImmediate(**iter);
		}
		for (auto iter = begin; iter != end; ++iter) {
			for (auto other = std::next(iter); other != end; ++other) {
				AddCombination(**iter, **other);
			}
		}
	}
};

void ExpressionStore::AddImmediate(const VariableDeclaration& decl) {
	auto declExpressions = MakeAllExpressions(decl);
	for (auto& expr : declExpressions) {
		if (!skipNull) expressions.push_back({ cola::copy(*expr), MakeNull() });
		expressions.push_back({ cola::copy(*expr), MakeBool(true) });
		expressions.push_back({ cola::copy(*expr), MakeBool(false) });
		expressions.push_back({ cola::copy(*expr), MakeMin() });
		expressions.push_back({ std::move(expr), MakeMax() });
	}
}

void ExpressionStore::AddCombination(const VariableDeclaration& decl, const VariableDeclaration& other) {
	auto declExpressions = MakeAllExpressions(decl);
	auto otherExpressions = MakeAllExpressions(other);
	for (const auto& declExpr : declExpressions) {
		for (const auto& otherExpr : otherExpressions) {
			expressions.push_back({ cola::copy(*declExpr), cola::copy(*otherExpr) });
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct CandidateGenerator {
	const SolverImpl& solver;
	const PostConfig& config;
	bool applyFilter;
	std::deque<std::unique_ptr<Axiom>> candidateAxioms;

	CandidateGenerator(const SolverImpl& solver, const PostConfig& config, bool applyFilter = plankton::config.filter_candidates_by_invariant)
		: solver(solver), config(config), applyFilter(applyFilter) {
			if (applyFilter) throw SolvingError("Filtering candidates by the invariant is not supported.");
	}

	void AddExpressions();
	void PopulateAxiom(std::unique_ptr<Axiom> axiom, bool addNegated=true);
	void AddOwnershipAxioms(const VariableDeclaration& decl);
	void AddSpecificationAxioms(const VariableDeclaration& decl);
	void AddLogicallyContainsAxioms(std::unique_ptr<Expression> expr);
	void AddHasFlowAxioms(std::unique_ptr<Expression> expr);
	void AddKeysetContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
	void AddFlowContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
	void AddAxioms();

	std::vector<Candidate> MakeCandidates();
};

inline std::vector<BinaryExpression::Operator> MakeOperators(const Expression& expr, const Expression& other) {
	using OP = BinaryExpression::Operator;
	if (expr.sort() != other.sort()) return {};
	if (expr.sort() == Sort::DATA) return { OP::EQ, OP::LT, OP::LEQ, OP::GT, OP::GEQ, OP::NEQ };
	return { OP::EQ, OP::NEQ };
}

void CandidateGenerator::AddExpressions() {
	ExpressionStore store(solver);
	for (auto& [lhs, rhs] : store.expressions) {
		// if (plankton::syntactically_equal(*lhs, *rhs)) continue;
		for (auto op : MakeOperators(*lhs, *rhs)) {
			candidateAxioms.push_back(MakeAxiom(MakeExpr(op, cola::copy(*lhs), cola::copy(*rhs))));
		}
	}
}

void CandidateGenerator::PopulateAxiom(std::unique_ptr<Axiom> axiom, bool addNegated) {
	if (addNegated) {
		candidateAxioms.push_back(plankton::copy(*axiom));
		axiom = MakeNot(std::move(axiom));
	}

	candidateAxioms.push_back(std::move(axiom));
}

void CandidateGenerator::AddOwnershipAxioms(const VariableDeclaration& decl) {
	if (decl.type.sort != Sort::PTR) return;
	PopulateAxiom(MakeOwnership(decl));
}

void CandidateGenerator::AddSpecificationAxioms(const VariableDeclaration& decl) {
	if (decl.type.sort != Sort::DATA) return;
	for (auto kind : SpecificationAxiom::KindIteratable) {
		PopulateAxiom(MakeObligation(kind, decl), false);
		PopulateAxiom(MakeFulfillment(kind, decl, true), false);
		PopulateAxiom(MakeFulfillment(kind, decl, false), false);
	}
}

void CandidateGenerator::AddLogicallyContainsAxioms(std::unique_ptr<Expression> expr) {
	if (expr->sort() != Sort::DATA) return;
	PopulateAxiom(MakeLogicallyContained(std::move(expr)));
}

void CandidateGenerator::AddHasFlowAxioms(std::unique_ptr<Expression> expr) {
	if (expr->sort() != Sort::PTR) return;
	PopulateAxiom(MakeHasFlow(std::move(expr)));
}

void CandidateGenerator::AddKeysetContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	PopulateAxiom(MakeKeysetContains(std::move(expr), std::move(other)));
}

void CandidateGenerator::AddFlowContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	PopulateAxiom(MakeFlowContains(std::move(expr), cola::copy(*other), std::move(other)));
}

void CandidateGenerator::AddAxioms() {
	for (auto var : solver.GetVariablesInScope()) {
		AddOwnershipAxioms(*var);
		AddSpecificationAxioms(*var);
	}

	for (auto var : solver.GetVariablesInScope()) {
		auto expressions = MakeAllExpressions(*var);
		for (auto& expr : expressions) {
			AddLogicallyContainsAxioms(cola::copy(*expr));
			AddHasFlowAxioms(std::move(expr));
		}
	}

	ExpressionStore store(solver, true);
	for (auto& [expr, other] : store.expressions) {
		AddKeysetContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddKeysetContainsAxioms(cola::copy(*other), cola::copy(*expr));
		AddFlowContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddFlowContainsAxioms(std::move(other), std::move(expr));
	}
}


std::vector<Candidate> CandidateGenerator::MakeCandidates() {
	log() << "### Creating candidates" << std::endl;

	// populate 'candidateAxioms'
	AddExpressions();
	AddAxioms();
	log() << "### created " << candidateAxioms.size() << " candidates" << std::endl;

	// make candidates
	std::vector<Candidate> result;
	result.reserve(candidateAxioms.size());
	for (const auto& axiom : candidateAxioms) {
		log() << "  - checking: " << *axiom << std::flush;
		ImplicationCheckerImpl checker(solver.GetEncoder(), *axiom);

		// find other candiate axioms that are implied by 'axiom'
		auto implied = std::make_unique<ConjunctionFormula>();
		for (const auto& other : candidateAxioms) {
			if (!checker.Implies(*other)) continue;
			implied->conjuncts.push_back(plankton::copy(*other));
		}
		log() << "  [" << implied->conjuncts.size() << "]" << std::flush;

		// find other candidate axioms that imply the negation of 'axiom'
		std::deque<std::unique_ptr<SimpleFormula>> disprove;
		for (const auto& other : candidateAxioms) {
			// A => B iff !B => !A; hence: B => !A iff A => !B
			if (!checker.ImpliesNegated(*other)) continue;
			disprove.push_back(MakeNot(plankton::copy(*other)));
		}
		log() << "  {" << disprove.size() << "}" << std::endl;

		// put together
		result.emplace_back(plankton::copy(*axiom), std::move(implied), std::move(disprove));
	}

	log() << "### checked candidate implications" << std::endl;

	std::sort(result.begin(), result.end(), [](const Candidate& lhs, const Candidate& rhs){
		if (lhs.GetImplied().conjuncts.size() > rhs.GetImplied().conjuncts.size()) return true;
		if (lhs.GetQuickDisprove().size() > rhs.GetQuickDisprove().size()) return true;
		return &lhs.GetCheck() > &rhs.GetCheck();
	});

	// TODO: adhere to applyFilter
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr std::string_view ERR_ENTER_PROGRAM = "Cannot enter program scope: program scope must be the outermost scope.";
static constexpr std::string_view ERR_ENTER_NOSCOPE = "Cannot enter scope: there must be an outermost program scope added first.";
static constexpr std::string_view ERR_LEAVE_NOSCOPE = "Cannot leave scope: there is no scope left to leave.";

inline void fail_if(bool fail, const std::string_view msg) {
	if (fail) {
		throw SolvingError(std::string(msg));
	}
}

void SolverImpl::ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars) {
	assert(!candidates.empty());
	assert(!variablesInScope.empty());
	assert(!instantiatedInvariants.empty());

	for (const auto& decl : vars) {
		variablesInScope.back().push_back(decl.get());

		if (cola::assignable(config.invariant->vars.at(0)->type, decl->type)) {
			instantiatedInvariants.back() = plankton::conjoin(std::move(instantiatedInvariants.back()), config.invariant->instantiate(*decl));
		}
	}

	// TODO important: ensure that the candidates can generate the invariant
	// TODO: reuse candidates
	auto newCandidates = CandidateGenerator(*this, config).MakeCandidates();
	candidates.back().insert(candidates.back().begin(), std::make_move_iterator(newCandidates.begin()), std::make_move_iterator(newCandidates.end()));
}

void SolverImpl::EnterScope(const cola::Program& program) {
	fail_if(!candidates.empty(), ERR_ENTER_PROGRAM);
	candidates.emplace_back();
	variablesInScope.emplace_back();
	instantiatedInvariants.push_back(std::make_unique<ConjunctionFormula>());

	ExtendCurrentScope(program.variables);
}

inline std::vector<Candidate> CopyCandidates(const std::vector<Candidate>& candidates) {
	std::vector<Candidate> result;
	result.reserve(candidates.size());
	for (const auto& candidate : candidates) {
		result.push_back(candidate.Copy());
	}
	return result;
}

void SolverImpl::PushOuterScope() {
	// copy from outermost scope (front)
	fail_if(candidates.empty(), ERR_ENTER_NOSCOPE);
	candidates.push_back(CopyCandidates(candidates.front()));
	instantiatedInvariants.push_back(plankton::copy(*instantiatedInvariants.front()));
	variablesInScope.push_back(std::vector<const VariableDeclaration*>(variablesInScope.front()));
}

void SolverImpl::PushInnerScope() {
	// copy from innermost scope (back)
	fail_if(candidates.empty(), ERR_ENTER_NOSCOPE);
	candidates.push_back(CopyCandidates(candidates.back()));
	instantiatedInvariants.push_back(plankton::copy(*instantiatedInvariants.back()));
	variablesInScope.push_back(std::vector<const VariableDeclaration*>(variablesInScope.back()));
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
	fail_if(candidates.empty(), ERR_LEAVE_NOSCOPE);
	instantiatedInvariants.pop_back();
	candidates.pop_back();
	variablesInScope.pop_back();
}
