#include "plankton/solver/solverimpl.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "plankton/config.hpp"
#include "plankton/error.hpp"
#include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, z3::solver solver_, const NowFormula& premise_, Encoder::StepTag tag_)
	: encoder(encoder_), solver(solver_), encodingTag(tag_), premiseNowFormula(&premise_), premiseAnnotation(nullptr) {
	solver.add(encoder.Encode(*premiseNowFormula, encodingTag));
}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, z3::solver solver_, const Annotation& premise_, Encoder::StepTag tag_)
	: encoder(encoder_), solver(solver_), encodingTag(tag_), premiseNowFormula(premise_.now.get()), premiseAnnotation(&premise_) {
	assert(premiseNowFormula);
	solver.add(encoder.Encode(*premiseNowFormula, encodingTag));
}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, const NowFormula& premise_, Encoder::StepTag tag_)
	: ImplicationCheckerImpl(encoder_, encoder_.MakeSolver(), premise_, tag_) {}

ImplicationCheckerImpl::ImplicationCheckerImpl(Encoder& encoder_, const Annotation& premise_, Encoder::StepTag tag_)
	: ImplicationCheckerImpl(encoder_, encoder_.MakeSolver(), premise_, tag_) {}


bool ImplicationCheckerImpl::Implies(z3::solver& solver, z3::expr expr) {
	// add negated 'expr' and check satisfiability
	solver.push();
	solver.add(!expr);
	auto result = solver.check();
	solver.pop();

	// analyse result
	switch (result) {
		case z3::unknown:
			if (plankton::config.z3_handle_unknown_result) return false;
			throw SolvingError("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");
		case z3::sat:
			return false;
		case z3::unsat:
			return true;
	}
}

bool ImplicationCheckerImpl::Implies(z3::expr expr) const {
	return Implies(solver, expr);
}


bool ImplicationCheckerImpl::Implies(const Expression& implied) const {
	if (implied.sort() != Sort::BOOL) {
		throw SolvingError("Cannot check implication for expression: expression is not of boolean sort.");
	}
	ExpressionAxiom axiom(cola::copy(implied));
	return Implies(axiom);
}

bool ImplicationCheckerImpl::ImpliesNonNull(const Expression& nonnull) const {
	BinaryExpression nonNull(BinaryExpression::Operator::NEQ, cola::copy(nonnull), std::make_unique<NullValue>());
	return Implies(nonNull);
}

bool ImplicationCheckerImpl::ImpliesFalse() const {
	BooleanValue value(false);
	return Implies(value);
}


struct Dispatcher : public LogicVisitor {
	const ImplicationCheckerImpl& checker;
	bool result;

	Dispatcher(const ImplicationCheckerImpl& checker) : checker(checker) {}

	bool Implies(const Formula& formula) {
		formula.accept(*this);
		return result;
	}

	void visit(const AxiomConjunctionFormula& formula) override { result = checker.Implies(formula); }
	void visit(const ImplicationFormula& formula) override { result = checker.Implies(formula); }
	void visit(const ConjunctionFormula& formula) override { result = checker.Implies(formula); }
	void visit(const NegatedAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const ExpressionAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const OwnershipAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const DataStructureLogicallyContainsAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const NodeLogicallyContainsAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const KeysetContainsAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const HasFlowAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const FlowContainsAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const ObligationAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const FulfillmentAxiom& formula) override { result = checker.Implies(formula); }
	void visit(const PastPredicate& formula) override { result = checker.Implies(formula); }
	void visit(const FuturePredicate& formula) override { result = checker.Implies(formula); }
	void visit(const Annotation& formula) override { result = checker.Implies(formula); }
};

bool ImplicationCheckerImpl::Implies(const Formula& implied) const {
	return Dispatcher(*this).Implies(implied);
}


struct QuickChecker : public BaseLogicVisitor {
	Encoder& encoder;
	Encoder::StepTag encodingTag;
	const NowFormula& premise;
	bool encode;
	z3::expr_vector conjuncts;
	bool discharged = true;

	QuickChecker(Encoder& encoder, Encoder::StepTag tag, const NowFormula& premise, bool encode=true)
		: encoder(encoder), encodingTag(tag), premise(premise), encode(encode), conjuncts(encoder.MakeVector()) {
			conjuncts.push_back(encoder.MakeTrue());
	}

	std::pair<bool, z3::expr_vector> CheckAndEncode(const NowFormula& implied) {
		implied.accept(*this);
		return { discharged , std::move(conjuncts) };
	}

	void handle(const SimpleFormula& formula) {
		if (!encode && !discharged) return;
		if (!plankton::syntactically_contains_conjunct(premise, formula)) {
			discharged = false;
			if (encode) {
				conjuncts.push_back(encoder.Encode(formula, encodingTag));
			}
		}
	}

	template<typename T>
	void handle_conjunction(const T& formula) {
		for (const auto& conjunct : formula.conjuncts) {
			handle(*conjunct);
		}
	}

	void visit(const ConjunctionFormula& formula) override { handle_conjunction(formula); }
	void visit(const AxiomConjunctionFormula& formula) override { handle_conjunction(formula); }

	void visit(const ImplicationFormula& formula) override { handle(formula); }
	void visit(const NegatedAxiom& formula) override { handle(formula); }
	void visit(const ExpressionAxiom& formula) override { handle(formula); }
	void visit(const OwnershipAxiom& formula) override { handle(formula); }
	void visit(const DataStructureLogicallyContainsAxiom& formula) override { handle(formula); }
	void visit(const NodeLogicallyContainsAxiom& formula) override { handle(formula); }
	void visit(const KeysetContainsAxiom& formula) override { handle(formula); }
	void visit(const HasFlowAxiom& formula) override { handle(formula); }
	void visit(const FlowContainsAxiom& formula) override { handle(formula); }
	void visit(const ObligationAxiom& formula) override { handle(formula); }
	void visit(const FulfillmentAxiom& formula) override { handle(formula); }
};


// bool ImplicationCheckerImpl::ImpliesQuick(const NowFormula& implied) const {
// 	assert(premiseNowFormula);
// 	auto [discharged, conjuncts] = QuickChecker(encoder, encodingTag, *premiseNowFormula).CheckAndEncode(implied);
// 	return discharged;
// }

bool ImplicationCheckerImpl::Implies(const NowFormula& implied) const {
	assert(premiseNowFormula);
	auto [discharged, conjuncts] = QuickChecker(encoder, encodingTag, *premiseNowFormula).CheckAndEncode(implied);
	discharged = discharged || Implies(encoder.MakeAnd(std::move(conjuncts)));
	// assert(discharged == Implies(encoder.Encode(implied, encodingTag)));
	return discharged;
}

bool ImplicationCheckerImpl::ImpliesNegated(const NowFormula& implied) const {
	return Implies(!encoder.Encode(implied, encodingTag));
}

bool ImplicationCheckerImpl::Implies(const PastPredicate& /*implied*/) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::Implies(const PastPredicate&)");
}

bool ImplicationCheckerImpl::Implies(const FuturePredicate& /*implied*/) const {
	throw std::logic_error("not yet implemented: ImplicationCheckerImpl::Implies(const FuturePredicate&)");
}

bool ImplicationCheckerImpl::Implies(const TimePredicate& implied) const {
	return Dispatcher(*this).Implies(implied);
}

bool ImplicationCheckerImpl::Implies(const Annotation& implied) const {
	if (!Implies(*implied.now)) return false;
	for (const auto& predicate : implied.time) {
		if (!Implies(*predicate)) return false;
	}
	return true;
}


std::vector<bool> ImplicationCheckerImpl::ComputeImplied(std::vector<const NowFormula*> formulas) const {
	static std::deque<VariableDeclaration> dummyVars;
	while (dummyVars.size() < 2*formulas.size()) {
		std::string name = "ImplicationCheckerImpl::ComputeImplied#dummy" + std::to_string(dummyVars.size());
		dummyVars.emplace_back(name, Type::bool_type(), false);
	}

	solver.push();
	auto assumptions = encoder.MakeVector();
	auto variables = encoder.MakeVector();
	auto consequences = encoder.MakeVector();
	for (std::size_t index = 0; index < formulas.size(); ++index) {
		auto [discharged, conjuncts] = QuickChecker(encoder, encodingTag, *premiseNowFormula).CheckAndEncode(*formulas.at(index));
		auto encodedFormula = encoder.MakeAnd(std::move(conjuncts));
		auto var = encoder.EncodeVariable(dummyVars.at(index), encodingTag);
		solver.add(var == encodedFormula);
		variables.push_back(var);
		// log() << "- var " << var << " --> " << *formulas.at(index) << std::endl;
	}
	auto res = solver.consequences(assumptions, variables, consequences);
	// log() << "CONSEQUENCES: " << res << " " << consequences << std::endl << std::endl;
	solver.pop();
	std::vector<bool> result(formulas.size());
	switch (res) {
		case z3::unknown:
			if (plankton::config.z3_handle_unknown_result) break;
			throw SolvingError("SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.");
		case z3::sat:
			break;
		case z3::unsat:
			result.flip();
			return result; // fast-path
	}
	for (std::size_t index = 0; index < formulas.size(); ++index) {
		auto searchFor = z3::implies(encoder.MakeTrue(), variables[index]);
		for (auto imp : consequences) {
			bool syntacticallyEqual = z3::eq(imp, searchFor);
			if (syntacticallyEqual) {
				// assert(Implies(*formulas.at(index)));
				result.at(index) = true;
				break;
			}
		}
		// assert(result.at(index) || !Implies(*formulas.at(index)));
	}

	return result;
}
