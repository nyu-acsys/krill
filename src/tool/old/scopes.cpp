#include "prover/solverimpl/linsolver.hpp"

#include "cola/util.hpp"
#include "heal/util.hpp"
#include "prover/config.hpp"
#include "prover/error.hpp"
#include "prover/logger.hpp" // TODO: delete

using namespace cola;
using namespace heal;
using namespace prover;
using var_list_t = const std::vector<const cola::VariableDeclaration*>&;

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<Expression>> MakeAllExpressions(const VariableDeclaration& decl) {
	std::vector<std::unique_ptr<Expression>> result;
	result.reserve(decl.type.fields.size() + 1);

	result.push_back(MakeExpr(decl));
	for (auto [declField, declFieldType] : decl.type.fields) {
		result.push_back(MakeDerefExpr(decl, declField));
	}
	return result;
}

std::vector<std::unique_ptr<Expression>> MakeAllImmediate(bool skipNull = false) {
	std::vector<std::unique_ptr<Expression>> result;
	result.reserve(5);
	if (!skipNull) result.push_back(MakeNullExpr());
	result.push_back(MakeBoolExpr(true));
	result.push_back(MakeBoolExpr(false));
	result.push_back(MakeMinExpr());
	result.push_back(MakeMaxExpr());
	return result;
}

enum struct CombinationKind { SINGLE, PAIR };

struct ExpressionStore {
	// all unordered distinct pairs of expressions
	std::deque<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> expressions;
	CombinationKind kind;
	bool skipNull;

	void AddImmediate(const VariableDeclaration& decl);
	void AddCombination(const VariableDeclaration& decl, const VariableDeclaration& other);
	void AddAllCombinations();
	
	ExpressionStore(var_list_t variables, CombinationKind kind, bool skipNull=false) : skipNull(skipNull) {
		auto begin = variables.begin();
		auto end = variables.end();

		switch (kind) {
			case CombinationKind::SINGLE:
				for (auto iter = begin; iter != end; ++iter) {
					AddImmediate(**iter);
				}
				break;

			case CombinationKind::PAIR:
				for (auto iter = begin; iter != end; ++iter) {
					for (auto other = std::next(iter); other != end; ++other) {
						AddCombination(**iter, **other);
					}
				}
				break;
		}
	}
};

void ExpressionStore::AddImmediate(const VariableDeclaration& decl) {
	auto declExpressions = MakeAllExpressions(decl);
	for (auto& expr : declExpressions) {
		auto otherExpressions = MakeAllImmediate(skipNull);
		for (auto& immi : otherExpressions) {
			expressions.push_back({ cola::copy(*expr), std::move(immi) });
		}
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

class Generator {
	private:
		std::shared_ptr<Encoder> encoderPtr;
		Encoder& encoder;
		var_list_t variables;
		CombinationKind kind;
		std::unique_ptr<ConjunctionFormula> candidates;
		std::unique_ptr<ConjunctionFormula> rules;

		void PopulateAxiom(std::unique_ptr<Axiom> axiom, bool addNegated=true);
		void PopulateRule(std::unique_ptr<ImplicationFormula> rule);
		void AddOwnershipAxioms(const VariableDeclaration& decl);
		void AddSpecificationAxioms(const VariableDeclaration& decl);
		void AddLogicallyContainsAxioms(std::unique_ptr<Expression> expr);
		void AddDSContainsAxioms(std::unique_ptr<Expression> expr);
		void AddHasFlowAxioms(std::unique_ptr<Expression> expr);
		void AddUniqueInflowAxioms(std::unique_ptr<Expression> expr);
		void AddNodeContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
		void AddKeysetContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
		void AddFlowContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
		
		void AddPairRules(const Expression& expr, const Expression& other);
		void AddOwnershipRules(const VariableDeclaration& expr, const Expression& other);
		void AddPureLinearizabilityRules(const VariableDeclaration& node, const VariableDeclaration& key);
		
		void AddExpressions();
		void AddCompoundSingles();
		void AddCompoundPairs();
		void AddCompound();

	public:
		Generator(std::shared_ptr<Encoder> encoderPtr, var_list_t variables, CombinationKind kind)
			: encoderPtr(encoderPtr), encoder(*encoderPtr), variables(variables), kind(kind),
			  candidates(std::make_unique<ConjunctionFormula>()), rules(std::make_unique<ConjunctionFormula>())
		{
			AddExpressions();
			AddCompound();
		}
		std::unique_ptr<ConjunctionFormula> MakeCandidates();
		std::unique_ptr<ConjunctionFormula> MakeRules();
};

inline std::vector<BinaryExpression::Operator> MakeOperators(const Expression& expr, const Expression& other) {
	using OP = BinaryExpression::Operator;
	if (expr.sort() != other.sort()) return {};
	if (expr.sort() == Sort::DATA) return { OP::EQ, OP::LT, OP::LEQ, OP::GT, OP::GEQ, OP::NEQ };
	return { OP::EQ, OP::NEQ };
}

void Generator::AddExpressions() {
	ExpressionStore store(variables, kind);
	for (const auto& [lhs, rhs] : store.expressions) {
		assert(lhs);
		assert(rhs);
		// if (prover::syntactically_equal(*lhs, *rhs)) continue;
		for (auto op : MakeOperators(*lhs, *rhs)) {
			if (heal::syntactically_equal(*lhs, *rhs)) continue;
			candidates->conjuncts.push_back(MakeAxiom(MakeExpr(op, cola::copy(*lhs), cola::copy(*rhs))));
		}
	}
}

void Generator::PopulateAxiom(std::unique_ptr<Axiom> axiom, bool addNegated) {
	if (addNegated) {
		candidates->conjuncts.push_back(heal::copy(*axiom));
		axiom = MakeNegatedAxiom(std::move(axiom));
	}

	candidates->conjuncts.push_back(std::move(axiom));
}

void Generator::PopulateRule(std::unique_ptr<ImplicationFormula> rule) {
	rules->conjuncts.push_back(std::move(rule));
}

void Generator::AddOwnershipAxioms(const VariableDeclaration& decl) {
	if (decl.type.sort != Sort::PTR) return;
	PopulateAxiom(MakeOwnershipAxiom(decl));
}

void Generator::AddSpecificationAxioms(const VariableDeclaration& decl) {
	if (decl.type.sort != Sort::DATA) return;
	for (auto kind : SpecificationAxiom::KindIteratable) {
		PopulateAxiom(MakeObligationAxiom(kind, decl), false);
		PopulateAxiom(MakeFulfillmentAxiom(kind, decl, true), false);
		PopulateAxiom(MakeFulfillmentAxiom(kind, decl, false), false);
	}
}

void Generator::AddDSContainsAxioms(std::unique_ptr<Expression> expr) {
	if (expr->sort() != Sort::DATA) return;
	PopulateAxiom(MakeDatastructureContainsAxiom(std::move(expr)));
}

void Generator::AddHasFlowAxioms(std::unique_ptr<Expression> expr) {
	if (expr->sort() != Sort::PTR) return;
	PopulateAxiom(MakeHasFlowAxiom(std::move(expr)));
}

void Generator::AddUniqueInflowAxioms(std::unique_ptr<Expression> expr) {
	if (expr->sort() != Sort::PTR) return;
	PopulateAxiom(MakeUniqueInflowAxiom(std::move(expr)));
}

void Generator::AddNodeContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	PopulateAxiom(MakeNodeContainsAxiom(std::move(expr), std::move(other)));
}

void Generator::AddKeysetContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	PopulateAxiom(MakeKeysetContainsAxiom(std::move(expr), std::move(other)));
}

void Generator::AddFlowContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	// TODO: add more?
	PopulateAxiom(MakeFlowContainsAxiom(std::move(expr), cola::copy(*other), std::move(other)));
}

void Generator::AddPureLinearizabilityRules(const VariableDeclaration& node, const VariableDeclaration& key) {
	if (node.type.sort != Sort::PTR || key.type.sort != Sort::DATA) return;
	auto AddRule = [&,this](SpecificationAxiom::Kind kind, bool contained, bool result){
		std::unique_ptr<Axiom> nodeContains = MakeNodeContainsAxiom(MakeExpr(node), MakeExpr(key));
		if (!contained) nodeContains = MakeNegatedAxiom(std::move(nodeContains));
		PopulateRule(MakeImplication(
			MakeAxiomConjunction(
				MakeObligationAxiom(kind, key),
				MakeKeysetContainsAxiom(MakeExpr(node), MakeExpr(key)),
				std::move(nodeContains)
			),
			MakeAxiomConjunction(
				MakeFulfillmentAxiom(kind, key, result)
			)
		));
	};

	AddRule(SpecificationAxiom::Kind::CONTAINS, true, true);
	AddRule(SpecificationAxiom::Kind::CONTAINS, false, false);
	AddRule(SpecificationAxiom::Kind::INSERT, true, false);
	AddRule(SpecificationAxiom::Kind::DELETE, false, false);
}

void Generator::AddOwnershipRules(const VariableDeclaration& node, const Expression& expr) {
	if (node.type.sort != Sort::PTR || expr.sort() != Sort::PTR) return;
	auto [exprIsVar, exprVar] = heal::is_of_type<VariableExpression>(expr);
	if (exprIsVar && !exprVar->decl.is_shared) return;

	PopulateRule(MakeImplication(
		MakeOwnershipAxiom(node), MakeAxiom(MakeExpr(BinaryExpression::Operator::NEQ, MakeExpr(node), cola::copy(expr)))
	));
}

void Generator::AddPairRules(const Expression& expr, const Expression& other) {
	auto [exprIsVar, exprVar] = heal::is_of_type<VariableExpression>(expr);
	if (!exprIsVar) return;

	AddOwnershipRules(exprVar->decl, other); // TODO important: needed

	auto [otherIsVar, otherVar] = heal::is_of_type<VariableExpression>(other);
	if (!otherIsVar) return;

	AddPureLinearizabilityRules(exprVar->decl, otherVar->decl);
}

void Generator::AddCompoundSingles() {
	// TODO: pass references and copy on demand
	for (auto var : variables) {
		AddOwnershipAxioms(*var);
		AddSpecificationAxioms(*var);
	}

	for (auto var : variables) {
		auto expressions = MakeAllExpressions(*var);
		for (auto& expr : expressions) {
			AddDSContainsAxioms(cola::copy(*expr));
			AddUniqueInflowAxioms(cola::copy(*expr));
			AddHasFlowAxioms(std::move(expr));
		}
	}
}

void Generator::AddCompoundPairs() {
	ExpressionStore store(variables, CombinationKind::PAIR, true);
	for (auto& [expr, other] : store.expressions) {
		AddPairRules(*expr, *other);
		AddPairRules(*other, *expr);

		// TODO: pass references and copy on demand
		AddNodeContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddNodeContainsAxioms(cola::copy(*other), cola::copy(*expr));
		AddKeysetContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddKeysetContainsAxioms(cola::copy(*other), cola::copy(*expr));
		AddFlowContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddFlowContainsAxioms(cola::copy(*other), cola::copy(*expr));
	}
}

void Generator::AddCompound() {
	switch (kind) {
		case CombinationKind::SINGLE: AddCompoundSingles(); break;
		case CombinationKind::PAIR: AddCompoundPairs(); break;
	}
}

std::unique_ptr<ConjunctionFormula> Generator::MakeCandidates() {
	assert(candidates);
	auto result = std::move(candidates);
	candidates.reset();
	return result;
}

std::unique_ptr<ConjunctionFormula> Generator::MakeRules() {
	assert(rules);
	auto result = std::move(rules);
	rules.reset();
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct Entry final {
	std::array<std::unique_ptr<VariableDeclaration>, 2> dummyVariables;
	std::unique_ptr<ConjunctionFormula> candidates;
	std::unique_ptr<ConjunctionFormula> rules;
};

class LazyStore {
	private:
		const SolverImpl& solver;
		std::map<std::pair<const Type*, const Type*>, Entry> entryMap;
		LazyStore(const SolverImpl& solver) : solver(solver) {}

		const Entry& GetEntry(const Type& type, const Type& other, CombinationKind kind);
		transformer_t MakeTransformer(const Entry& entry, const VariableDeclaration& decl, const VariableDeclaration& other);
		std::unique_ptr<ConjunctionFormula> InstantiateRawCandidates(const VariableDeclaration& decl, const VariableDeclaration& other, CombinationKind kind);
		std::unique_ptr<ConjunctionFormula> InstantiateRawRules(const VariableDeclaration& decl, const VariableDeclaration& other);
		std::unique_ptr<ConjunctionFormula> MakeSingleCandidates(const VariableDeclaration& decl);
		std::unique_ptr<ConjunctionFormula> MakePairCandidates(const VariableDeclaration& decl, const VariableDeclaration& other);
		std::unique_ptr<ConjunctionFormula> MakeCandidates();
		std::unique_ptr<ConjunctionFormula> MakeRules();

	public:
		static LazyStore& GetStore(const SolverImpl& solver);
		static std::unique_ptr<ConjunctionFormula> MakeCandidates(const SolverImpl& solver);
		static std::unique_ptr<ConjunctionFormula> MakeRules(const SolverImpl& solver);
};

const Entry& LazyStore::GetEntry(const Type& type, const Type& other, CombinationKind kind) {
	// search for existing
	auto key = std::make_pair(&type, &other);
	auto find = entryMap.find(key);
	if (find != entryMap.end()) return find->second;

	// create new
	Entry entry;
	entry.dummyVariables[0] = std::make_unique<VariableDeclaration>("LazyStore#dummy-0", type, false);
	entry.dummyVariables[1] = std::make_unique<VariableDeclaration>("LazyStore#dummy-1", other, false);
	std::vector<const VariableDeclaration*> vars = { entry.dummyVariables.at(0).get(), entry.dummyVariables.at(1).get() };
	Generator generator(solver.GetEncoder(), vars, kind);
	entry.candidates = generator.MakeCandidates();
	entry.rules = generator.MakeRules();

	// add
	auto insert = entryMap.emplace(key, std::move(entry));
	assert(insert.second);
	return insert.first->second;
}

transformer_t LazyStore::MakeTransformer(const Entry& entry, const VariableDeclaration& decl, const VariableDeclaration& other) {
	std::vector<const VariableDeclaration*> from = { entry.dummyVariables.at(0).get(), entry.dummyVariables.at(1).get() };
	std::vector<const VariableDeclaration*> to = { &decl, &other };
	return [search=std::move(from),replace=std::move(to)](const Expression& expr){
		std::pair<bool,std::unique_ptr<cola::Expression>> result;
		auto [isVar, var] = heal::is_of_type<VariableExpression>(expr);
		if (isVar) {
			for (std::size_t index = 0; index < search.size(); ++index) {
				if (&var->decl != search.at(index)) continue;
				result.first = true;
				result.second = std::make_unique<VariableExpression>(*replace.at(index));
				break;
			}
		}
		return result;
	};
}

std::unique_ptr<ConjunctionFormula> LazyStore::InstantiateRawCandidates(const VariableDeclaration& decl, const VariableDeclaration& other, CombinationKind kind) {
	const auto& entry = GetEntry(decl.type, other.type, kind);
	auto transformer = MakeTransformer(entry, decl, other);
	return heal::replace_expression(heal::copy(*entry.candidates), transformer);
}

std::unique_ptr<ConjunctionFormula> LazyStore::InstantiateRawRules(const VariableDeclaration& decl, const VariableDeclaration& other) {
	const auto& entry = GetEntry(decl.type, other.type, CombinationKind::PAIR);
	auto transformer = MakeTransformer(entry, decl, other);
	return heal::replace_expression(heal::copy(*entry.rules), transformer);
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakeSingleCandidates(const VariableDeclaration& decl) {
	static Type dummyType("LazyStore#MakeSingleCandidates#dummy-type", Sort::VOID);
	static VariableDeclaration dummyDecl("LazyStore#MakeSingleCandidates#dummy-decl", dummyType, false);
	return InstantiateRawCandidates(decl, dummyDecl, CombinationKind::SINGLE);
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakePairCandidates(const VariableDeclaration& decl, const VariableDeclaration& other) {
	if (&decl.type > &other.type) return MakePairCandidates(other, decl);
	return InstantiateRawCandidates(decl, other, CombinationKind::PAIR);
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakeCandidates() {
	const auto& variables = solver.GetVariablesInScope();
	auto candidates = std::make_unique<ConjunctionFormula>();
	auto AddCandidates = [&candidates](std::unique_ptr<ConjunctionFormula> newCandidates) {
		candidates = heal::conjoin(std::move(candidates), std::move(newCandidates));
	};

	// add false
	candidates->conjuncts.push_back(MakeAxiom(MakeFalseExpr()));

	// singleton candidates
	for (auto var : variables) {
		AddCandidates(MakeSingleCandidates(*var));
	}

	// pair candidates (distinct unsorted pairs)
	for (auto iter = variables.begin(); iter != variables.end(); ++iter) {
		for (auto other = std::next(iter); other != variables.end(); ++other) {
			AddCandidates(MakePairCandidates(**iter, **other));
		}
	}

	// done
	return candidates;
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakeRules() {
	const auto& variables = solver.GetVariablesInScope();
	auto result = std::make_unique<ConjunctionFormula>();
	for (auto iter = variables.begin(); iter != variables.end(); ++iter) {
		for (auto other = std::next(iter); other != variables.end(); ++other) {
			result = heal::conjoin(std::move(result), InstantiateRawRules(**iter, **other));
		}
	}
	return result;
}

LazyStore& LazyStore::GetStore(const SolverImpl& solver) {
	static std::map<const SolverImpl*, LazyStore> lookup;
	auto find = lookup.find(&solver);
	if (find != lookup.end()) return find->second;
	auto insertion = lookup.emplace(&solver, LazyStore(solver));
	return insertion.first->second;
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakeCandidates(const SolverImpl& solver) {
	return GetStore(solver).MakeCandidates();
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakeRules(const SolverImpl& solver) {
	return GetStore(solver).MakeRules();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// std::vector<std::unique_ptr<Expression>> SolverImpl::MakeCandidateExpressions(cola::Sort sort) const {
// 	std::vector<std::unique_ptr<Expression>> result;
// 	auto immiExpressions = MakeAllImmediate();
// 	for (auto& immi : immiExpressions) {
// 		if (immi->sort() != sort) continue;
// 		result.push_back(std::move(immi));
// 	}
// 	for (auto decl : GetVariablesInScope()) {
// 		auto declExpressions = MakeAllExpressions(*decl);
// 		for (auto& expr : declExpressions) {
// 			if (expr->sort() != sort) continue;
// 			result.push_back(std::move(expr));
// 		}
// 	}
// 	return result;
// }


static constexpr std::string_view ERR_LEAVE_NOSCOPE = "Cannot leave scope: there is no scope left to leave.";

inline void fail_if(bool fail, const std::string_view msg) {
	if (fail) {
		throw SolvingError(std::string(msg));
	}
}

void SolverImpl::ExtendCurrentScope(const std::vector<std::unique_ptr<cola::VariableDeclaration>>& decls) {
	assert(!candidates.empty());
	assert(!variablesInScope.empty());
	assert(!instantiatedRules.empty());
	assert(!instantiatedInvariants.empty());

	// add variables to scope
	for (const auto& decl : decls) {
		variablesInScope.back().push_back(decl.get());
	}

	// instantiate invariant (for all variables in scope, not just for newly addedd ones)
	for (auto decl : GetVariablesInScope()) {
		if (cola::assignable(config.invariant->vars.at(0)->type, decl->type)) {
			instantiatedInvariants.back() = heal::conjoin(std::move(instantiatedInvariants.back()), config.invariant->instantiate(*decl));
		}
	}

	// add candidates
	candidates.back() = LazyStore::MakeCandidates(*this);

	// add rules
	auto rules = LazyStore::MakeRules(*this);
	instantiatedRules.back() = heal::conjoin(std::move(instantiatedRules.back()), std::move(rules));
}

void SolverImpl::PushScope() {
	candidates.emplace_back();
	instantiatedRules.push_back(std::make_unique<ConjunctionFormula>());
	instantiatedInvariants.push_back(std::make_unique<ConjunctionFormula>());
	if (variablesInScope.empty()) {
		// create empty scope
		variablesInScope.emplace_back();
	} else {
		// copy over variables previously in scope
		variablesInScope.push_back(std::vector<const VariableDeclaration*>(variablesInScope.back()));
	}
}

inline void RemoveVariablesFromScope(std::vector<const cola::VariableDeclaration*>& scope, const Macro& macro) {
	// remove variables from 'scope' that are assigend to by 'macro'
	// TODO: remove variables that are not live
	scope.erase(
		std::remove_if(std::begin(scope), std::end(scope), [&macro](const VariableDeclaration* decl) {
			for (const VariableDeclaration& lhs : macro.lhs) {
				if (&lhs == decl) return true;
			}
			return false;
		}),
		std::end(scope)
	);
}

void SolverImpl::EnterScope(const Program& program) {
	PushScope();
	ExtendCurrentScope(program.variables);
}

void SolverImpl::EnterScope(const Function& function) {
	PushScope();
	ExtendCurrentScope(function.args);
}

void SolverImpl::EnterScope(const Scope& scope) {
	PushScope();
	ExtendCurrentScope(scope.variables);
}

void SolverImpl::EnterScope(const Macro& macro) {
	PushScope();
	RemoveVariablesFromScope(variablesInScope.back(), macro);
	ExtendCurrentScope(macro.decl.args);
}

void SolverImpl::LeaveScope() {
	fail_if(candidates.empty(), ERR_LEAVE_NOSCOPE);
	instantiatedRules.pop_back();
	instantiatedInvariants.pop_back();
	candidates.pop_back();
	variablesInScope.pop_back();
}
