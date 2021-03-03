#include "prover/solver.hpp"

#include <set>
#include "heal/util.hpp"
#include "prover/error.hpp"
#include "prover/solverimpl/linsolver.hpp"

using namespace cola;
using namespace heal;
using namespace prover;


std::unique_ptr<Solver> prover::MakeLinearizabilitySolver(const cola::Program& program) {
	return prover::MakeSolverImpl(program);
}


struct PropertyChecker : public BaseVisitor, public DefaultListener {
	using BaseVisitor::visit;
	using DefaultListener::visit;
	using DefaultListener::enter;

	const Type& nodeType;
	PropertyChecker(const Type& nodeType) : nodeType(nodeType) {}

	bool inside_deref = false;
	std::set<const VariableDeclaration*> allowedVars;
	std::string propertyType;
	std::string propertyName;

	void throwup(std::string reasong) {
		throw SolvingError("Malformed " + propertyType + " '" + propertyName + "': " + reasong + ".");
	}

	template<typename T>
	void check(const T& property) {
		inside_deref = false;
		if (property.vars.size() != 1 && property.vars.size() != 2) {
			throw SolvingError("Unexpected property '" + property.name + "': expected an 'Invariant' or 'Predicate'.");
		}
		propertyType = property.vars.size() == 1 ? "invariant" : "predicate";
		propertyName = property.name;
		allowedVars.clear();
		for (const auto& var : property.vars) {
			allowedVars.insert(var.get());
		}
		if (property.vars.size() >= 1) {
			if (!property.vars.at(0)) throwup("first argument is undefined");
			if (&property.vars.at(0)->type != &nodeType) throwup("type of first argument must be '" + nodeType.name + "'");
		}
		if (property.vars.size() >= 2) {
			if (!property.vars.at(1)) throwup("second argument is undefined");
			if (property.vars.at(1)->type.sort != Sort::DATA) throwup("sort of second argument must be 'DATA'");
		}
		property.blueprint->accept(*this);
	}

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
	void enter(const ExpressionAxiom& formula) override { formula.expr->accept(*this); }
	void enter(const OwnershipAxiom& formula) override { formula.expr->accept(*this); }
	void enter(const DataStructureLogicallyContainsAxiom& formula) override { formula.value->accept(*this); }
	void enter(const NodeLogicallyContainsAxiom& formula) override { formula.node->accept(*this); formula.value->accept(*this); }
	void enter(const KeysetContainsAxiom& formula) override { formula.node->accept(*this); formula.value->accept(*this); }
	void enter(const HasFlowAxiom& formula) override { formula.expr->accept(*this); }
	void enter(const FlowContainsAxiom& formula) override { formula.node->accept(*this); formula.value_low->accept(*this); formula.value_high->accept(*this); }
	void enter(const UniqueInflowAxiom& formula) override { formula.node->accept(*this); }
	
	void visit(const VariableExpression& node) override {
		if (node.decl.is_shared) return;
		if (allowedVars.count(&node.decl) > 0) return;
		throwup("non-shared non-argument variable '" + node.decl.name + "' not supported");
	}

	void visit(const Dereference& node) override {
		if (inside_deref) {
			throwup("nested dereferences are not supported");
		}
		inside_deref = true;
		node.expr->accept(*this);
		inside_deref = false;
	}

	void enter(const ObligationAxiom& /*formula*/) override {
		throwup("proof obligations are not supported");
	}

	void enter(const FulfillmentAxiom& /*formula*/) override {
		throwup("proof obligation fulfillments are not supported");
	}
};


Solver::Solver(PostConfig config_) : config(std::move(config_)) {
	assert(config.flowDomain);
	assert(config.logicallyContainsKey);
	assert(config.invariant);

	// TODO: sanity check for 'config'
	const Type& nodeType = config.flowDomain->GetNodeType();
	PropertyChecker checker(nodeType);
	checker.check(*config.invariant);
	checker.check(*config.logicallyContainsKey);
	for (auto [fieldName, fieldType] : nodeType.fields) {
		if (fieldType.get().sort != Sort::PTR) continue;
		checker.check(config.flowDomain->GetOutFlowContains(fieldName));
	}
}

std::unique_ptr<Annotation> Solver::Join(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) const {
	std::vector<std::unique_ptr<Annotation>> vec;
	vec.reserve(2);
	vec.push_back(std::move(annotation));
	vec.push_back(std::move(other));
	return Join(std::move(vec));
}

bool Solver::UncheckedPostEntails(const ConjunctionFormula& pre, const cola::Assignment& cmd, const ConjunctionFormula& post) const {
	Annotation preAnnotation(heal::copy(pre));
	auto postAnnotation = Post(preAnnotation, cmd);
	return Implies(*postAnnotation, post);
}

bool Solver::ImpliesFalse(const heal::Formula& formula) const {
	return MakeImplicationChecker(formula)->ImpliesFalse();
}

bool Solver::ImpliesFalseQuick(const heal::Formula& formula) const {
	ExpressionAxiom falseAxiom(std::make_unique<BooleanValue>(false));
	return heal::syntactically_contains_conjunct(formula, falseAxiom);
}

bool Solver::Implies(const heal::Formula& formula, const heal::Formula& implied) const {
	return MakeImplicationChecker(formula)->Implies(implied);
}

bool Solver::Implies(const heal::Formula& formula, const cola::Expression& implied) const {
	return MakeImplicationChecker(formula)->Implies(implied);
}

bool Solver::ImpliesIsNull(const heal::Formula& formula, const cola::Expression& expression) const {
	return MakeImplicationChecker(formula)->ImpliesIsNull(expression);
}

bool Solver::ImpliesIsNonNull(const heal::Formula& formula, const cola::Expression& expression) const {
	return MakeImplicationChecker(formula)->ImpliesIsNonNull(expression);
}
