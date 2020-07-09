#include "plankton/solver.hpp"

#include <iostream> // TODO: delete
#include "plankton/solver/solverimpl.hpp"

using namespace cola;
using namespace plankton;


template<typename F>
std::unique_ptr<Invariant> mk_invariant(const Type& nodeType, std::string name, F make_blueprint) {
	std::vector<std::unique_ptr<VariableDeclaration>> vars;
	vars.push_back(std::make_unique<VariableDeclaration>("\%ptr", nodeType, false));
	auto blueprint = make_blueprint(*vars.at(0));
	return std::make_unique<Invariant>(name, std::move(vars), std::move(blueprint));
}

template<typename F>
std::unique_ptr<Predicate> mk_predicate(const Type& nodeType, std::string name, F make_blueprint) {
	std::vector<std::unique_ptr<VariableDeclaration>> vars;
	vars.push_back(std::make_unique<VariableDeclaration>("\%ptr", nodeType, false));
	vars.push_back(std::make_unique<VariableDeclaration>("\%key", Type::data_type(), false));
	auto blueprint = make_blueprint(*vars.at(0), *vars.at(1));
	return std::make_unique<Predicate>(name, std::move(vars), std::move(blueprint));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Expression> mk_non_null(std::unique_ptr<Expression> expr) {
	return std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::NEQ, std::move(expr), std::make_unique<NullValue>()
	);
}

std::unique_ptr<ExpressionAxiom> mk_axiom(std::unique_ptr<Expression> expr) {
	return std::make_unique<ExpressionAxiom>(std::move(expr));
}

std::unique_ptr<Predicate> make_dummy_outflow(const Program& /*program*/, const Type& nodeType) {
	std::cout << "MAKING DUMMY OUTFLOW PREDICATE" << std::endl;
	return mk_predicate(nodeType, "^OUTFLOW", [](const auto& node, const auto& key){
		auto result = std::make_unique<AxiomConjunctionFormula>();
		result->conjuncts.push_back(mk_axiom(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::LT,
			std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "val"),
			std::make_unique<VariableExpression>(key)
		)));
		result->conjuncts.push_back(std::make_unique<FlowContainsAxiom>(
			std::make_unique<VariableExpression>(node),
			std::make_unique<VariableExpression>(key),
			std::make_unique<VariableExpression>(key)
		));
		return result;
	});
}

std::unique_ptr<Predicate> make_dummy_contains(const Program& /*program*/, const Type& nodeType) {
	std::cout << "MAKING DUMMY CONTAINS PREDICATE" << std::endl;
	return mk_predicate(nodeType, "^CONTAINS", [](const auto& node, const auto& key){
		auto equals = std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::EQ,
			std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "val"),
			std::make_unique<VariableExpression>(key)
		);
		auto unmarked = mk_non_null(std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "next"));
		auto result = std::make_unique<AxiomConjunctionFormula>();
		result->conjuncts.push_back(mk_axiom(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::AND, std::move(equals), std::move(unmarked)
		)));
		return result;
	});
}

std::unique_ptr<Invariant> make_dummy_invariant(const Program& program, const Type& nodeType) {
	std::cout << "MAKING DUMMY INVARIANT" << std::endl;
	return mk_invariant(nodeType, "^INVARIANT", [&program](const auto& node){
		auto result = std::make_unique<ConjunctionFormula>();
		const auto& head = *program.variables.at(0);
		result->conjuncts.push_back(mk_axiom(mk_non_null(std::make_unique<VariableExpression>(head))));
		result->conjuncts.push_back(mk_axiom(mk_non_null(std::make_unique<Dereference>(std::make_unique<VariableExpression>(head), "next"))));
		result->conjuncts.push_back(mk_axiom(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::EQ, std::make_unique<Dereference>(std::make_unique<VariableExpression>(head), "val"), std::make_unique<MinValue>()
		)));
		result->conjuncts.push_back(std::make_unique<ImplicationFormula>(
			mk_axiom(std::make_unique<BinaryExpression>(BinaryExpression::Operator::NEQ, std::make_unique<VariableExpression>(head), std::make_unique<VariableExpression>(node))),
			mk_axiom(std::make_unique<BinaryExpression>(BinaryExpression::Operator::LT, std::make_unique<MinValue>(), std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "val")))
		));
		result->conjuncts.push_back(std::make_unique<ImplicationFormula>(
			std::make_unique<NegatedAxiom>(std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(node))),
			std::make_unique<KeysetContainsAxiom>(std::make_unique<VariableExpression>(node), std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "val"))
		));
		return result;
	});
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


// struct DereferenceChecker : BaseVisitor {
// 	bool result = true;
// 	bool inside_deref = false;

// 	static bool is_node_local(const Expression& expr) {
// 		DereferenceChecker visitor;
// 		expr.accept(visitor);
// 		return visitor.result;
// 	}

// 	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
// 	void visit(const NullValue& /*node*/) override { /* do nothing */ }
// 	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
// 	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
// 	void visit(const MinValue& /*node*/) override { /* do nothing */ }
// 	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
// 	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }

// 	void visit(const NegatedExpression& node) override {
// 		if (result) node.expr->accept(*this);
// 	}
// 	void visit(const BinaryExpression& node) override {
// 		if (result) node.lhs->accept(*this);
// 		if (result) node.rhs->accept(*this);
// 	}
// 	void visit(const Dereference& node) override {
// 		if (inside_deref) result = false;
// 		if (!result) return;
// 		inside_deref = true;
// 		node.expr->accept(*this);
// 		inside_deref = false;
// 	}
// };

const Type& extract_node_type(const Program& program) {
	if (program.types.size() != 1) {
		throw std::logic_error("Cannot extract node type from program.");
	}
	return *program.types.at(0);
}

// std::string extract_flow_field(const Type& nodeType) {
// 	std::string fieldname;
// 	bool found = false;
// 	for (auto [name, fieldtype] : nodeType.fields) {
// 		if (&fieldtype.get() == &nodeType) {
// 			fieldname = name;
// 			if (found) {
// 				throw std::logic_error("Incompatible node type: too many pointer fields.");
// 			}
// 			found = true;
// 		}
// 	}
// 	if (!found) {
// 		throw std::logic_error("Cannot find pointer field in node type.");
// 	}
// 	return fieldname;
// }

std::unique_ptr<Predicate> extract_outflow_predicate(const Program& program, const Type& nodeType) {
	return make_dummy_outflow(program, nodeType);

	// TODO: check node locality
	throw std::logic_error("not yet implemented: extract_outflow_predicate");
}

std::unique_ptr<Predicate> extract_contains_predicate(const Program& program, const Type& nodeType) {
	return make_dummy_contains(program, nodeType);

	// TODO: check node locality
	throw std::logic_error("not yet implemented: extract_contains_predicate");
}


std::unique_ptr<Invariant> extract_invariant(const Program& program, const Type& nodeType) {
	return make_dummy_invariant(program, nodeType);

	// TODO: check node locality
	throw std::logic_error("not yet implemented: extract_invariant");
	// for (const auto& inv : source.invariants) {
	// 	if (inv->vars.size() > 1) {
	// 		throw std::logic_error("Cannot construct node invariant from given program, invariant '" + inv->name + "' is malformed: expected at most one variable but got " + std::to_string(inv->vars.size()) + ".");
	// 	}
	// 	if (inv->vars.size() == 1 && inv->vars.at(0)->type.sort != Sort::PTR) {
	// 		throw std::logic_error("Cannot construct node invariant from given program, invariant '" + inv->name + "' is malformed: variable is not of pointer sort.");
	// 	}
	// 	if (!DereferenceChecker::is_node_local(*inv->expr)) {
	// 		throw std::logic_error("Cannot construct node invariant from given program, invariant '" + inv->name + "' is not node-local.");
	// 	}
	// }
}

struct KeysetFlow : public FlowDomain {
	const Type& nodeType;
	std::unique_ptr<Predicate> outflowContains;

	KeysetFlow(const Type& type_, std::unique_ptr<Predicate> predicate) : nodeType(type_), outflowContains(std::move(predicate)) {}

	const Type& GetNodeType() const override {
		return nodeType;
	}

	const Predicate& GetOutFlowContains(std::string fieldname) const override {
		if (fieldname != "next") throw std::logic_error("KeysetFlow error: FlowDomain.GetOutFlowContains(std::string) expected 'next' but got '" + fieldname + "'.");
		return *outflowContains;
	}

	std::size_t GetFootprintSize(const Annotation& /*pre*/, const Dereference& lhs, const Expression& /*rhs*/) const override {
		return lhs.sort() == Sort::PTR ? 3 : 1;
	}
};

std::unique_ptr<Solver> plankton::MakeLinearizabilitySolver(const Program& program) {
	auto& nodeType = extract_node_type(program);
	PostConfig config {
		std::make_unique<KeysetFlow>(nodeType, extract_outflow_predicate(program, nodeType)),
		extract_contains_predicate(program, nodeType),
		extract_invariant(program, nodeType)
	};
	return std::make_unique<SolverImpl>(std::move(config));
}



