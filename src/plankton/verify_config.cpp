#include "plankton/verify.hpp"

#include <iostream> // TODO: delete
#include "plankton/config.hpp"

using namespace cola;
using namespace plankton;
using OutFlowInfo = plankton::PlanktonConfig::OutFlowInfo;


template<typename F>
Invariant mk_invariant(const Type& nodeType, std::string name, F make_blueprint) {
	std::array<std::unique_ptr<VariableDeclaration>, 1> vars {
		std::make_unique<VariableDeclaration>("\%ptr", nodeType, false)
	};
	auto blueprint = make_blueprint(*vars.at(0));
	return Invariant(name, std::move(vars), std::move(blueprint));
}

template<typename F>
Predicate mk_predicate(const Type& nodeType, std::string name, F make_blueprint) {
	std::array<std::unique_ptr<VariableDeclaration>, 2> vars {
		std::make_unique<VariableDeclaration>("\%ptr", nodeType, false),
		std::make_unique<VariableDeclaration>("\%key", Type::data_type(), false)
	};
	auto blueprint = make_blueprint(*vars.at(0), *vars.at(1));
	return Predicate(name, std::move(vars), std::move(blueprint));
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

Predicate make_dummy_outflow(const Program& /*program*/, const Type& nodeType) {
	std::cout << "MAKING DUMMY OUTFLOW PREDICATE" << std::endl;
	return mk_predicate(nodeType, "^OUTFLOW", [](const auto& node, const auto& key){
		return mk_axiom(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::LT,
			std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "val"),
			std::make_unique<VariableExpression>(key)
		));
	});
}

Predicate make_dummy_contains(const Program& /*program*/, const Type& nodeType) {
	std::cout << "MAKING DUMMY CONTAINS PREDICATE" << std::endl;
	return mk_predicate(nodeType, "^CONTAINS", [](const auto& node, const auto& key){
		auto equals = std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::EQ,
			std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "val"),
			std::make_unique<VariableExpression>(key)
		);
		auto unmarked = mk_non_null(std::make_unique<Dereference>(std::make_unique<VariableExpression>(node), "next"));
		return mk_axiom(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::AND, std::move(equals), std::move(unmarked)
		));
	});
}

Invariant make_dummy_invariant(const Program& program, const Type& nodeType) {
	std::cout << "MAKING DUMMY INVARIANT" << std::endl;
	return mk_invariant(nodeType, "^INVARIANT", [&program](const auto& /*node*/){
		auto result = std::make_unique<ConjunctionFormula>();
		const auto& head = *program.variables.at(0);
		result->conjuncts.push_back(mk_axiom(mk_non_null(std::make_unique<VariableExpression>(head))));
		result->conjuncts.push_back(mk_axiom(mk_non_null(std::make_unique<Dereference>(std::make_unique<VariableExpression>(head), "next"))));
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

std::string extract_flow_field(const Type& nodeType) {
	std::string fieldname;
	bool found = false;
	for (auto [name, fieldtype] : nodeType.fields) {
		if (&fieldtype.get() == &nodeType) {
			fieldname = name;
			if (found) {
				throw std::logic_error("Incompatible node type: too many pointer fields.");
			}
			found = true;
		}
	}
	if (!found) {
		throw std::logic_error("Cannot find pointer field in node type.");
	}
	return fieldname;
}

std::vector<OutFlowInfo> extract_outflow_info(const Program& program, const Type& nodeType, std::string fieldname) {
	// auto pred = make_dummy_outflow(program, nodeType);
	// std::cout << pred.vars.at(0)->type.name << std::endl;
	// std::cout << nodeType.name << std::endl;
	// std::cout << &pred.vars.at(0)->type.name << std::endl;
	// std::cout << &nodeType.name << std::endl;
	// OutFlowInfo(nodeType, fieldname, pred);
	return { OutFlowInfo(nodeType, fieldname, make_dummy_outflow(program, nodeType)) };

	// TODO: check node locality
	throw std::logic_error("not yet implemented: extract_outflow_predicate");
}

Predicate extract_contains_predicate(const Program& program, const Type& nodeType) {
	return make_dummy_contains(program, nodeType);

	// TODO: check node locality
	throw std::logic_error("not yet implemented: extract_contains_predicate");
}


Invariant extract_invariant(const Program& program, const Type& nodeType) {
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


struct VerificationConfig final : public PlanktonConfig {
	const Type& node_type;
	const std::string flow_field;
	std::vector<OutFlowInfo> outflow_info;
	Predicate contains;
	Invariant invariant;

	VerificationConfig(const Program& program)
		: node_type(extract_node_type(program)),
		  flow_field(extract_flow_field(node_type)),
		  outflow_info(extract_outflow_info(program, node_type, flow_field)),
		  contains(extract_contains_predicate(program, node_type)),
		  invariant(extract_invariant(program, node_type))
	{}
	
	bool is_flow_field_local(const Type& /*nodeType*/, std::string /*fieldname*/) override {
		return true;
	}

	bool is_flow_insertion_inbetween_local(const Type& /*nodeType*/, std::string /*fieldname*/) override {
		return true;
	}

	bool is_flow_unlinking_local(const Type& /*nodeType*/, std::string /*fieldname*/) override {
		return true;
	}

	const std::vector<OutFlowInfo>& get_outflow_info() override {
		return outflow_info;
	}

	const Predicate& get_logically_contains_key() override {
		return contains;
	}

	const Invariant& get_invariant() override {
		return invariant;
	}
};


void Verifier::set_config(const Program& program) {
	plankton::config = std::make_unique<VerificationConfig>(program);
}



