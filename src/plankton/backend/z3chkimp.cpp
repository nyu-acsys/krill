#include "plankton/backend/z3chkimp.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


Z3ImplicationChecker::Z3ImplicationChecker(std::shared_ptr<Z3Encoder> encoder_, EncodingTag tag_, z3::solver solver_)
	: encoderPtr(encoder_), encoder(*encoderPtr), encodingTag(tag_), solver(std::move(solver_))
	{}

void Z3ImplicationChecker::Push() {
	solver.push();
}

void Z3ImplicationChecker::Pop() {
	solver.pop();
}

void Z3ImplicationChecker::AddPremise(Z3Expr expr) {
	solver.add(expr);
}

void Z3ImplicationChecker::AddPremise(Term term) {
	solver.add(encoder.Internalize(term));
}

void Z3ImplicationChecker::AddPremise(const Formula& premise) {
	solver.add(encoder.EncodeZ3(premise, encodingTag));
}


bool Z3ImplicationChecker::Implies(Z3Expr expr) const {
	// add negated 'expr' and check satisfiability
	solver.push();
	solver.add(expr.Neg());
	auto result = solver.check();
	solver.pop();

	// analyse result
	switch (result) {
		case z3::unknown:
			throw Z3SolvingError("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");
		case z3::sat:
			return false;
		case z3::unsat:
			return true;
	}
}

bool Z3ImplicationChecker::Implies(Term term) const {
	return Implies(encoder.Internalize(term));
}

bool Z3ImplicationChecker::Implies(const Formula& implied) const {
	return Implies(encoder.EncodeZ3(implied, encodingTag));
}

bool Z3ImplicationChecker::Implies(const Expression& implied) const {
	return Implies(encoder.EncodeZ3(implied, encodingTag));
}


bool Z3ImplicationChecker::ImpliesFalse() const {
	return Implies(encoder.MakeZ3False());
}

bool Z3ImplicationChecker::ImpliesNegated(Term term) const {
	return Implies(encoder.Internalize(term).Neg());
}

bool Z3ImplicationChecker::ImpliesNegated(const NowFormula& implied) const {
	return Implies(encoder.EncodeZ3(implied, encodingTag).Neg());
}


bool Z3ImplicationChecker::ImpliesIsNull(Term term) const {
	return Implies(encoder.Internalize(term).Equal(encoder.MakeZ3NullPtr()));
}

bool Z3ImplicationChecker::ImpliesIsNonNull(Term term) const {
	return Implies(encoder.Internalize(term).Distinct(encoder.MakeZ3NullPtr()));
}

bool Z3ImplicationChecker::ImpliesIsNull(const Expression& expression) const {
	return Implies(encoder.EncodeZ3(expression, encodingTag).Equal(encoder.MakeZ3NullPtr()));
}

bool Z3ImplicationChecker::ImpliesIsNonNull(const Expression& expression) const {
	return Implies(encoder.EncodeZ3(expression, encodingTag).Distinct(encoder.MakeZ3NullPtr()));
}


std::vector<bool> Z3ImplicationChecker::ComputeImplied(const std::vector<Term>& terms) const {
	// convert
	std::vector<Z3Expr> exprs;
	exprs.reserve(terms.size());
	for (auto term : terms) {
		exprs.push_back(encoder.Internalize(term));
	}

	// check
	return ComputeImplied(exprs);
}

std::unique_ptr<ConjunctionFormula> Z3ImplicationChecker::ComputeImpliedConjuncts(const ConjunctionFormula& formula) const {
	// convert into internal representation
	std::vector<Z3Expr> exprs;
	exprs.reserve(formula.conjuncts.size());
	for (const auto& conjunct : formula.conjuncts) {
		exprs.push_back(encoder.EncodeZ3(*conjunct, encodingTag));
	}

	// check
	auto implied = ComputeImplied(exprs);
	assert(implied.size() == formula.conjuncts.size());

	// create result
	auto result = std::make_unique<ConjunctionFormula>();
	for (std::size_t index = 0; index < implied.size(); ++index) {
		if (!implied.at(index)) continue;
		result->conjuncts.push_back(heal::copy(*formula.conjuncts.at(index)));
	}
	return result;
}


static const std::deque<VariableDeclaration>& GetDummyDecls(std::size_t amount) {
	static std::deque<VariableDeclaration> dummyVars;
	while (dummyVars.size() < amount) {
		std::string name = "Z3ImplicationChecker#dummy" + std::to_string(dummyVars.size());
		dummyVars.emplace_back(name, Type::bool_type(), false);
	}
	return dummyVars;
}

inline z3::expr_vector GetVariables(Z3Encoder& encoder, EncodingTag tag, std::size_t amount) {
	auto& decls = GetDummyDecls(amount);
	auto result = encoder.MakeZ3ExprVector();
	for (std::size_t index = 0; index < amount; ++index) {
		result.push_back(encoder.EncodeZ3(decls.at(index), tag));
	}
	assert(result.size() == amount);
	return result;
}

std::vector<bool> Z3ImplicationChecker::ComputeImplied(const std::vector<Z3Expr>& exprs) const {
	// save state
	solver.push();

	// prepare required vectors
	auto variables = GetVariables(encoder, encodingTag, exprs.size());
	auto assumptions = encoder.MakeZ3ExprVector();
	auto consequences = encoder.MakeZ3ExprVector();

	// prepare solver
	for (std::size_t index = 0; index < exprs.size(); ++index) {
		solver.add(variables[index] == exprs.at(index).expr);
	}

	// check 
	auto answer = solver.consequences(assumptions, variables, consequences);

	// reset state
	solver.pop();

	// create result
	std::vector<bool> result(exprs.size());
	switch (answer) {
		case z3::unknown:
			throw Z3SolvingError("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");

		case z3::unsat:
			result.flip();
			return result;

		case z3::sat:
			for (std::size_t index = 0; index < exprs.size(); ++index) {
				auto searchFor = z3::implies(encoder.MakeZ3True(), variables[index]);
				for (auto implication : consequences) {
					bool syntacticallyEqual = z3::eq(implication, searchFor);
					if (!syntacticallyEqual) continue;
					result.at(index) = true;
					break;
				}
			}
			return result;
	}
}
