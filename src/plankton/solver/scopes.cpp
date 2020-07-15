#include "plankton/solver/solverimpl.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "cola/util.hpp"
#include "plankton/config.hpp"
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;
using var_list_t = const std::vector<const cola::VariableDeclaration*>&;


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
	
	ExpressionStore(var_list_t variables, bool skipNull=false) : skipNull(skipNull) {
		auto begin = variables.begin();
		auto end = variables.end();
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
	if (&decl == &other) return;
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
	Encoder& encoder;
	const PostConfig& config;
	var_list_t variables;
	bool applyFilter;
	std::deque<std::unique_ptr<Axiom>> candidateAxioms;

	CandidateGenerator(Encoder& encoder, const PostConfig& config, var_list_t variables, bool applyFilter=plankton::config.filter_candidates_by_invariant)
		: encoder(encoder), config(config), variables(variables), applyFilter(applyFilter) {}

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
	ExpressionStore store(variables);
	for (const auto& [lhs, rhs] : store.expressions) {
		assert(lhs);
		assert(rhs);
		// if (plankton::syntactically_equal(*lhs, *rhs)) continue;
		for (auto op : MakeOperators(*lhs, *rhs)) {
			if (plankton::syntactically_equal(*lhs, *rhs)) continue;
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
	for (auto var : variables) {
		AddOwnershipAxioms(*var);
		AddSpecificationAxioms(*var);
	}

	for (auto var : variables) {
		auto expressions = MakeAllExpressions(*var);
		for (auto& expr : expressions) {
			AddLogicallyContainsAxioms(cola::copy(*expr));
			AddHasFlowAxioms(std::move(expr));
		}
	}

	ExpressionStore store(variables, true);
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
	log() << "### created " << candidateAxioms.size() << " candidates" << std::flush;

	// make candidates
	std::vector<Candidate> result;
	result.reserve(candidateAxioms.size());
	for (const auto& axiom : candidateAxioms) {
		log() << "." << std::flush;
		ImplicationCheckerImpl checker(encoder, *axiom);

		// find other candiate axioms that are implied by 'axiom'
		auto implied = std::make_unique<ConjunctionFormula>();
		for (const auto& other : candidateAxioms) {
			if (!checker.Implies(*other)) continue;
			implied->conjuncts.push_back(plankton::copy(*other));
		}

		// find other candidate axioms that imply the negation of 'axiom'
		std::deque<std::unique_ptr<SimpleFormula>> disprove;
		for (const auto& other : candidateAxioms) {
			// A => B iff !B => !A; hence: B => !A iff A => !B
			if (!checker.ImpliesNegated(*other)) continue; // TODO: important: this does not seem to work properly <<<<<<=====================
			disprove.push_back(MakeNot(plankton::copy(*other)));
		}

		// put together
		result.emplace_back(plankton::copy(*axiom), std::move(implied), std::move(disprove));
	}

	log() << std::endl << "### checked candidate implications" << std::endl;

	std::sort(result.begin(), result.end(), [](const Candidate& lhs, const Candidate& rhs){
		if (lhs.GetImplied().conjuncts.size() > rhs.GetImplied().conjuncts.size()) return true;
		if (lhs.GetQuickDisprove().size() > rhs.GetQuickDisprove().size()) return true;
		return &lhs.GetCheck() > &rhs.GetCheck();
	});

	// filter
	if (applyFilter) {
		// TODO: implement
		throw SolvingError("Filtering candidates by invariant is not supported.");
	}
	
	// done
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

using type_pair_t = std::pair<const Type*, const Type*>;
using decl_pair_t = std::pair<const VariableDeclaration*, const VariableDeclaration*>;

class CandidateStore {
	private:
		const SolverImpl& solver;
		const PostConfig& config;
		std::vector<std::unique_ptr<VariableDeclaration>> dummyDeclarations;
		std::map<type_pair_t, decl_pair_t> type2decl;
		std::map<decl_pair_t, std::vector<Candidate>> decl2candidate;

		CandidateStore(const SolverImpl& solver, const PostConfig& config) : solver(solver), config(config) {}
		type_pair_t MakeTypeKey(const Type& type, const Type& other);
		decl_pair_t MakeDeclKey(const Type& type, const Type& other);
		const std::vector<Candidate>& GetRawCandidates(decl_pair_t key);
		transformer_t MakeTransformer(decl_pair_t key, const VariableDeclaration& decl, const VariableDeclaration& other);
		std::vector<Candidate> MakeCandidates(const VariableDeclaration& decl, const VariableDeclaration& other);
		std::vector<Candidate> MakeCandidates();

	public:
		static CandidateStore& GetStore(const SolverImpl& solver, const PostConfig& config);
		static std::vector<Candidate> MakeCandidates(const SolverImpl& solver, const PostConfig& config);
};

type_pair_t CandidateStore::MakeTypeKey(const Type& type, const Type& other) {
	return std::make_pair(&type, &other);
}

decl_pair_t CandidateStore::MakeDeclKey(const Type& type, const Type& other) {
	auto key = MakeTypeKey(type, other);
	auto find = type2decl.find(key);
	if (find != type2decl.end()) return find->second;
	auto first = std::make_unique<VariableDeclaration>("CandidateStore#" + type.name + "$" + other.name + "#first", type, false);
	auto second = std::make_unique<VariableDeclaration>("CandidateStore#" + type.name + "$" + other.name + "#second", other, false);
	auto result = std::make_pair(first.get(), second.get());
	dummyDeclarations.push_back(std::move(first));
	dummyDeclarations.push_back(std::move(second));
	type2decl[key] = result;
	return result;
}

const std::vector<Candidate>& CandidateStore::GetRawCandidates(decl_pair_t key) {
	auto find = decl2candidate.find(key);
	if (find != decl2candidate.end()) return find->second;

	log() << "||| creating raw candidates:  " << key.first->name << "  " << key.second->name << std::endl;
	std::vector<const VariableDeclaration*> varList = { key.first, key.second };
	auto candidates = CandidateGenerator(solver.GetEncoder(), config, varList).MakeCandidates();
	auto insert = decl2candidate.emplace(key, std::move(candidates));
	assert(insert.second);
	return insert.first->second;
}

transformer_t CandidateStore::MakeTransformer(decl_pair_t key, const VariableDeclaration& decl, const VariableDeclaration& other) {
	return [key,&decl,&other](const Expression& expr){
		std::pair<bool,std::unique_ptr<cola::Expression>> result;
		auto [isVar, var] = plankton::is_of_type<VariableExpression>(expr);
		if (isVar) {
			if (&var->decl == key.first) {
				result.first = true;
				result.second = std::make_unique<VariableExpression>(decl);
			} else if (&var->decl == key.second) {
				result.first = true;
				result.second = std::make_unique<VariableExpression>(other);
			}
		}
		return result;
	};
}

std::vector<Candidate> CandidateStore::MakeCandidates(const VariableDeclaration& decl, const VariableDeclaration& other) {
	if (&decl.type > &other.type) return MakeCandidates(other, decl);
	auto key = MakeDeclKey(decl.type, other.type);
	const auto& rawCandidates = GetRawCandidates(key);
	auto transformer = MakeTransformer(key, decl, other);

	std::vector<Candidate> result;
	for (const auto& raw : rawCandidates) {
		auto check = plankton::replace_expression(plankton::copy(raw.GetCheck()), transformer);
		auto implied = plankton::replace_expression(plankton::copy(raw.GetImplied()), transformer);
		std::deque<std::unique_ptr<SimpleFormula>> disprove;
		for (const auto& formula : raw.GetQuickDisprove()) {
			disprove.push_back(plankton::replace_expression(plankton::copy(*formula), transformer));
		}
		result.emplace_back(std::move(check), std::move(implied), std::move(disprove));
	}
	
	return result;
}

std::vector<Candidate> CandidateStore::MakeCandidates() {
	std::vector<Candidate> result;
	const auto& variables = solver.GetVariablesInScope();

	// handle special case
	// TODO: improve ==> add dummy variables of type void to the outermost scope?
	if (variables.size() == 1) {
		static VariableDeclaration singletonDummy("CandidateStore#dummy", Type::void_type(), false);
		result = MakeCandidates(*variables.at(0), singletonDummy);
	}

	// add all distinct unsorted combinations
	for (auto iter = variables.begin(); iter != variables.end(); ++iter) {
		for (auto other = std::next(iter); other != variables.end(); ++other) {
			auto candidates = MakeCandidates(**iter, **other);
			result.insert(result.end(), std::make_move_iterator(candidates.begin()), std::make_move_iterator(candidates.end()));

			// std::vector<Candidate> take; // TODO: is this needed?
			// for (auto& candidate : candidates) {
			// 	for (const auto& existing : result) {
			// 		if (plankton::syntactically_equal(candidate.GetCheck(), existing.GetCheck())) {
			// 			log() << " pruned candidate: " << candidate.GetCheck() << std::endl;
			// 			continue;
			// 		}
			// 		take.push_back(std::move(candidate));
			// 	}
			// }
			// result.insert(result.end(), std::make_move_iterator(take.begin()), std::make_move_iterator(take.end()));
		}
	}

	auto source = std::move(result);
	result.clear();
	for (auto iter = source.begin(); iter != source.end(); ++iter) {
		bool drop = false;
		for (auto other = std::next(iter); other != source.end(); ++other) {
			if (plankton::syntactically_equal(iter->GetCheck(), other->GetCheck())) {
				drop = true;
				break;
			}
		}
		if (drop) continue;
		result.push_back(std::move(*iter));
	}

	// log() << "Create candidates " << result.size() << std::endl;
	// for (const auto& candidate : result) {
	// 	log() << "  - " << candidate.GetCheck() << std::endl;
	// }
	// TODO important: remove duplicates <<<<<<==========================================================================================
	// TODO important: sort result <<<<<<================================================================================================
	// throw std::logic_error("pointu breaku");

	// done
	return result;
}

CandidateStore& CandidateStore::GetStore(const SolverImpl& solver, const PostConfig& config) {
	static std::map<std::pair<const SolverImpl*, const PostConfig*>, CandidateStore> lookup;
	auto key = std::make_pair(&solver, &config);
	auto find = lookup.find(key);
	if (find != lookup.end()) return find->second;
	auto insertion = lookup.emplace(key, CandidateStore(solver, config));
	return insertion.first->second;
}

std::vector<Candidate> CandidateStore::MakeCandidates(const SolverImpl& solver, const PostConfig& config) {
	return GetStore(solver, config).MakeCandidates();
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
	// TODO: reuse old candidates?
	auto newCandidates = CandidateStore::MakeCandidates(*this, config);
	candidates.back() = std::move(newCandidates);
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
