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
inline std::unique_ptr<DataStructureLogicallyContainsAxiom> MakeDSContains(std::unique_ptr<Expression> value) { return std::make_unique<DataStructureLogicallyContainsAxiom>(std::move(value)); }
inline std::unique_ptr<NodeLogicallyContainsAxiom> MakeNodeContains(std::unique_ptr<Expression> node, std::unique_ptr<Expression> value) { return std::make_unique<NodeLogicallyContainsAxiom>(std::move(node), std::move(value)); }
inline std::unique_ptr<HasFlowAxiom> MakeHasFlow(std::unique_ptr<Expression> expr) { return std::make_unique<HasFlowAxiom>(std::move(expr)); }
inline std::unique_ptr<KeysetContainsAxiom> MakeKeysetContains(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) { return std::make_unique<KeysetContainsAxiom>(std::move(expr), std::move(other)); }
inline std::unique_ptr<FlowContainsAxiom> MakeFlowContains(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other, std::unique_ptr<Expression> more) { return std::make_unique<FlowContainsAxiom>(std::move(expr), std::move(other), std::move(more)); }

struct MakeImplication {
	std::unique_ptr<ImplicationFormula> result;
	MakeImplication() : result(std::make_unique<ImplicationFormula>()) {}
	MakeImplication& Premise() { return *this; }
	MakeImplication& Conclusion() { return *this; }
	template<typename... T>
	MakeImplication& Premise(std::unique_ptr<Axiom> axiom, T... rest) {
		result->premise->conjuncts.push_back(std::move(axiom));
		Premise(std::forward<T>(rest)...);
		return *this;
	}
	template<typename... T>
	MakeImplication& Conclusion(std::unique_ptr<Axiom> axiom, T... rest) {
		result->conclusion->conjuncts.push_back(std::move(axiom));
		Conclusion(std::forward<T>(rest)...);
		return *this;
	}
	template<typename... T>
	MakeImplication(std::unique_ptr<Axiom> axiom, T... rest) : MakeImplication() { Premise(std::move(axiom), std::forward<T>(rest)...); }
	std::unique_ptr<ImplicationFormula> Get() { return std::move(result); }
};

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

class Generator {
	private:
		Encoder& encoder;
		var_list_t variables;
		CombinationKind kind;
		std::deque<std::unique_ptr<Axiom>> candidateAxioms;
		std::unique_ptr<ConjunctionFormula> rules;

		void PopulateAxiom(std::unique_ptr<Axiom> axiom, bool addNegated=true);
		void PopulateRule(std::unique_ptr<ImplicationFormula> rule);
		void AddOwnershipAxioms(const VariableDeclaration& decl);
		void AddSpecificationAxioms(const VariableDeclaration& decl);
		void AddLogicallyContainsAxioms(std::unique_ptr<Expression> expr);
		void AddDSContainsAxioms(std::unique_ptr<Expression> expr);
		void AddHasFlowAxioms(std::unique_ptr<Expression> expr);
		void AddNodeContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
		void AddKeysetContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
		void AddFlowContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
		void AddPureLinearizabilityRules(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other);
		void AddPureLinearizabilityRules(const VariableDeclaration& node, const VariableDeclaration& key);
		
		void AddExpressions();
		void AddCompoundSingles();
		void AddCompoundPairs();
		void AddCompound();

	public:
		Generator(Encoder& encoder, var_list_t variables, CombinationKind kind)
			: encoder(encoder), variables(variables), kind(kind), rules(std::make_unique<ConjunctionFormula>())
		{
			AddExpressions();
			AddCompound();
		}
		std::vector<Candidate> MakeCandidates();
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
		// if (plankton::syntactically_equal(*lhs, *rhs)) continue;
		for (auto op : MakeOperators(*lhs, *rhs)) {
			if (plankton::syntactically_equal(*lhs, *rhs)) continue;
			candidateAxioms.push_back(MakeAxiom(MakeExpr(op, cola::copy(*lhs), cola::copy(*rhs))));
		}
	}
}

void Generator::PopulateAxiom(std::unique_ptr<Axiom> axiom, bool addNegated) {
	if (addNegated) {
		candidateAxioms.push_back(plankton::copy(*axiom));
		axiom = MakeNot(std::move(axiom));
	}

	candidateAxioms.push_back(std::move(axiom));
}

void Generator::PopulateRule(std::unique_ptr<ImplicationFormula> rule) {
	rules->conjuncts.push_back(std::move(rule));
}

void Generator::AddOwnershipAxioms(const VariableDeclaration& decl) {
	if (decl.type.sort != Sort::PTR) return;
	PopulateAxiom(MakeOwnership(decl));
}

void Generator::AddSpecificationAxioms(const VariableDeclaration& decl) {
	if (decl.type.sort != Sort::DATA) return;
	for (auto kind : SpecificationAxiom::KindIteratable) {
		PopulateAxiom(MakeObligation(kind, decl), false);
		PopulateAxiom(MakeFulfillment(kind, decl, true), false);
		PopulateAxiom(MakeFulfillment(kind, decl, false), false);
	}
}

void Generator::AddDSContainsAxioms(std::unique_ptr<Expression> expr) {
	if (expr->sort() != Sort::DATA) return;
	PopulateAxiom(MakeDSContains(std::move(expr)));
}

void Generator::AddHasFlowAxioms(std::unique_ptr<Expression> expr) {
	if (expr->sort() != Sort::PTR) return;
	PopulateAxiom(MakeHasFlow(std::move(expr)));
}

void Generator::AddNodeContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	PopulateAxiom(MakeNodeContains(std::move(expr), std::move(other)));
}

void Generator::AddKeysetContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	PopulateAxiom(MakeKeysetContains(std::move(expr), std::move(other)));
}

void Generator::AddFlowContainsAxioms(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	PopulateAxiom(MakeFlowContains(std::move(expr), cola::copy(*other), std::move(other)));
}

void Generator::AddPureLinearizabilityRules(const VariableDeclaration& node, const VariableDeclaration& key) {
	auto AddRule = [&,this](SpecificationAxiom::Kind kind, bool contained, bool result){
		std::unique_ptr<Axiom> nodeContains = MakeNodeContains(MakeExpr(node), MakeExpr(key));
		if (!contained) nodeContains = MakeNot(std::move(nodeContains));
		PopulateRule(MakeImplication(
			MakeObligation(kind, key),
			MakeKeysetContains(MakeExpr(node), MakeExpr(key)),
			std::move(nodeContains)
		).Conclusion(
			MakeFulfillment(kind, key, result)
		).Get());
	};

	AddRule(SpecificationAxiom::Kind::CONTAINS, true, true);
	AddRule(SpecificationAxiom::Kind::CONTAINS, false, false);
	AddRule(SpecificationAxiom::Kind::INSERT, true, false);
	AddRule(SpecificationAxiom::Kind::DELETE, false, false);
}

void Generator::AddPureLinearizabilityRules(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> other) {
	if (expr->sort() != Sort::PTR || other->sort() != Sort::DATA) return;
	auto [exprIsVar, exprVar] = plankton::is_of_type<VariableExpression>(*expr);
	if (!exprIsVar) return;
	auto [otherIsVar, otherVar] = plankton::is_of_type<VariableExpression>(*other);
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
			AddHasFlowAxioms(std::move(expr));
		}
	}
}

void Generator::AddCompoundPairs() {
	ExpressionStore store(variables, CombinationKind::PAIR, true);
	for (auto& [expr, other] : store.expressions) {
		// TODO: pass references and copy on demand
		AddNodeContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddNodeContainsAxioms(cola::copy(*other), cola::copy(*expr));
		AddKeysetContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddKeysetContainsAxioms(cola::copy(*other), cola::copy(*expr));
		AddFlowContainsAxioms(cola::copy(*expr), cola::copy(*other));
		AddFlowContainsAxioms(cola::copy(*other), cola::copy(*expr));

		AddPureLinearizabilityRules(cola::copy(*expr), cola::copy(*other));
		AddPureLinearizabilityRules(std::move(other), std::move(expr));
	}
}

void Generator::AddCompound() {
	switch (kind) {
		case CombinationKind::SINGLE: AddCompoundSingles(); break;
		case CombinationKind::PAIR: AddCompoundPairs(); break;
	}
}

std::vector<Candidate> Generator::MakeCandidates() {
	// TODO important: remove those candidates that are implied by the invariant?
	log() << "### Creating candidates (" << (kind == CombinationKind::SINGLE ? "single" : "pair") << ")..." << std::flush;

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

		// put together
		result.emplace_back(plankton::copy(*axiom), std::move(implied));
	}
	log() << result.size() << std::endl;

	// sort
	std::sort(result.begin(), result.end(), [](const Candidate& lhs, const Candidate& rhs){
		if (lhs.GetImplied().conjuncts.size() > rhs.GetImplied().conjuncts.size()) return true;
		return &lhs.GetCheck() > &rhs.GetCheck();
	});
	
	// done
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
	std::vector<Candidate> candidates;
	std::unique_ptr<ConjunctionFormula> rules;
};

class LazyStore {
	private:
		const SolverImpl& solver;
		std::map<std::pair<const Type*, const Type*>, Entry> entryMap;
		LazyStore(const SolverImpl& solver) : solver(solver) {}

		const Entry& GetEntry(const Type& type, const Type& other, CombinationKind kind);
		transformer_t MakeTransformer(const Entry& entry, const VariableDeclaration& decl, const VariableDeclaration& other);
		std::vector<Candidate> InstantiateRawCandidates(const VariableDeclaration& decl, const VariableDeclaration& other, CombinationKind kind);
		std::unique_ptr<ConjunctionFormula> InstantiateRawRules(const VariableDeclaration& decl, const VariableDeclaration& other);
		std::vector<Candidate> MakeSingleCandidates(const VariableDeclaration& decl);
		std::vector<Candidate> MakePairCandidates(const VariableDeclaration& decl, const VariableDeclaration& other);
		std::vector<Candidate> MakeCandidates();
		std::unique_ptr<ConjunctionFormula> MakeRules();

	public:
		static LazyStore& GetStore(const SolverImpl& solver);
		static std::vector<Candidate> MakeCandidates(const SolverImpl& solver);
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
		auto [isVar, var] = plankton::is_of_type<VariableExpression>(expr);
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

std::vector<Candidate> LazyStore::InstantiateRawCandidates(const VariableDeclaration& decl, const VariableDeclaration& other, CombinationKind kind) {
	const auto& entry = GetEntry(decl.type, other.type, kind);
	auto transformer = MakeTransformer(entry, decl, other);

	std::vector<Candidate> result;
	result.reserve(entry.candidates.size());
	for (const auto& candidate : entry.candidates) {
		result.emplace_back(plankton::replace_expression(plankton::copy(*candidate.repr), transformer));
	}
	return result;
}

std::unique_ptr<ConjunctionFormula> LazyStore::InstantiateRawRules(const VariableDeclaration& decl, const VariableDeclaration& other) {
	const auto& entry = GetEntry(decl.type, other.type, CombinationKind::PAIR);
	auto transformer = MakeTransformer(entry, decl, other);
	return plankton::replace_expression(plankton::copy(*entry.rules), transformer);
}

std::vector<Candidate> LazyStore::MakeSingleCandidates(const VariableDeclaration& decl) {
	static Type dummyType("LazyStore#MakeSingleCandidates#dummy-type", Sort::VOID);
	static VariableDeclaration dummyDecl("LazyStore#MakeSingleCandidates#dummy-decl", dummyType, false);
	return InstantiateRawCandidates(decl, dummyDecl, CombinationKind::SINGLE);
}

std::vector<Candidate> LazyStore::MakePairCandidates(const VariableDeclaration& decl, const VariableDeclaration& other) {
	if (&decl.type > &other.type) return MakePairCandidates(other, decl);
	return InstantiateRawCandidates(decl, other, CombinationKind::PAIR);
}

std::vector<Candidate> LazyStore::MakeCandidates() {
	const auto& variables = solver.GetVariablesInScope();
	std::deque<Candidate> candidates;
	auto AddCandidates = [&candidates](std::vector<Candidate>&& newCandidates) {
		candidates.insert(candidates.end(), std::make_move_iterator(newCandidates.begin()), std::make_move_iterator(newCandidates.end()));
	};

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

	// create false
	std::vector<Candidate> result;
	result.reserve(candidates.size() + 1);
	bool addFalse = plankton::config.add_false_candidate;
	if (addFalse) result.emplace_back(std::make_unique<ExpressionAxiom>(std::make_unique<BooleanValue>(false)));

	// prune candidates and complete false
	auto IsContained = [&result](const Candidate& search) {
		return std::find_if(result.cbegin(), result.cend(), [&search](const Candidate& elem) {
			return plankton::syntactically_equal(search.GetCheck(), elem.GetCheck());
		}) != result.end();
	};
	for (auto& candidate : candidates) {
		if (IsContained(candidate)) continue;
		if (addFalse) result.front().repr = plankton::conjoin(std::move(result.front().repr), plankton::copy(*candidate.repr));
		result.push_back(std::move(candidate));
	}

	// done
	return result;
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakeRules() {
	const auto& variables = solver.GetVariablesInScope();
	auto result = std::make_unique<ConjunctionFormula>();
	for (auto iter = variables.begin(); iter != variables.end(); ++iter) {
		for (auto other = std::next(iter); other != variables.end(); ++other) {
			result = plankton::conjoin(std::move(result), InstantiateRawRules(**iter, **other));
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

std::vector<Candidate> LazyStore::MakeCandidates(const SolverImpl& solver) {
	return GetStore(solver).MakeCandidates();
}

std::unique_ptr<ConjunctionFormula> LazyStore::MakeRules(const SolverImpl& solver) {
	return GetStore(solver).MakeRules();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

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
			instantiatedInvariants.back() = plankton::conjoin(std::move(instantiatedInvariants.back()), config.invariant->instantiate(*decl));
		}
	}

	// add candidates and rules
	candidates.back() = LazyStore::MakeCandidates(*this);
	instantiatedRules.back() = LazyStore::MakeRules(*this);
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
