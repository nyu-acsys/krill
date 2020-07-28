#include "plankton/solverimpl/linsolver.hpp"

#include "heal/util.hpp"
#include "plankton/logger.hpp" // TODO: remove

using namespace cola;
using namespace heal;
using namespace plankton;


template<typename F>
std::unique_ptr<Invariant> mk_invariant(const Type& nodeType, std::string name, F make_blueprint) {
	auto ptr = std::make_unique<VariableDeclaration>("\%ptr", nodeType, false);
	auto blueprint = make_blueprint(*ptr);
	std::array<std::unique_ptr<VariableDeclaration>, 1> vars = { std::move(ptr) };
	return std::make_unique<Invariant>(name, std::move(vars), std::move(blueprint));
}

template<typename F>
std::unique_ptr<Predicate> mk_predicate(const Type& nodeType, std::string name, F make_blueprint) {
	auto ptr = std::make_unique<VariableDeclaration>("\%ptr", nodeType, false);
	auto key = std::make_unique<VariableDeclaration>("\%key", Type::data_type(), false);
	auto blueprint = make_blueprint(*ptr, *key);
	std::array<std::unique_ptr<VariableDeclaration>, 2> vars = { std::move(ptr), std::move(key) };
	return std::make_unique<Predicate>(name, std::move(vars), std::move(blueprint));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Expression> mk_non_null(std::unique_ptr<Expression> expr) {
	return MakeExpr(BinaryExpression::Operator::NEQ, std::move(expr), MakeNullExpr());
}

std::unique_ptr<Predicate> make_dummy_outflow(const Program& /*program*/, const Type& nodeType) {
	std::cout << "MAKING DUMMY OUTFLOW PREDICATE" << std::endl;
	return mk_predicate(nodeType, "^OUTFLOW", [](const auto& node, const auto& key){
		return MakeAxiomConjunction(
			MakeAxiom(MakeExpr(BinaryExpression::Operator::LT, MakeDerefExpr(node, "val"), MakeExpr(key))),
			MakeFlowContainsAxiom(MakeExpr(node), MakeExpr(key), MakeExpr(key))
		);
	});
}

std::unique_ptr<Predicate> make_dummy_contains(const Program& /*program*/, const Type& nodeType) {
	std::cout << "MAKING DUMMY CONTAINS PREDICATE" << std::endl;
	return mk_predicate(nodeType, "^CONTAINS", [](const auto& node, const auto& key){
		auto equals = MakeExpr(BinaryExpression::Operator::EQ, MakeDerefExpr(node, "val"), MakeExpr(key));
		auto unmarked = mk_non_null(MakeDerefExpr(node, "next"));
		return MakeAxiomConjunction(
			MakeAxiom(std::move(equals)),
			MakeAxiom(std::move(unmarked))
		);
	});
}

std::unique_ptr<Invariant> make_dummy_invariant(const Program& program, const Type& nodeType) {
	std::cout << "MAKING DUMMY INVARIANT" << std::endl;
	return mk_invariant(nodeType, "^INVARIANT", [&program](const auto& node){
		const auto& head = *program.variables.at(0);
		return MakeConjunction(
			MakeAxiom(mk_non_null(MakeExpr(head))),
			MakeAxiom(mk_non_null(MakeDerefExpr(head, "next"))),
			MakeAxiom(MakeExpr(BinaryExpression::Operator::EQ, MakeDerefExpr(head, "val"), MakeMinExpr())),
			MakeFlowContainsAxiom(MakeExpr(head), MakeMinExpr(), MakeMaxExpr()),
			MakeImplication(
				MakeAxiom(MakeExpr(BinaryExpression::Operator::NEQ, MakeExpr(head), MakeExpr(node))),
				MakeAxiom(MakeExpr(BinaryExpression::Operator::LT, MakeMinExpr(), MakeDerefExpr(node, "val")))
			),
			MakeImplication(
				MakeAxiomConjunction(
					MakeNegatedAxiom(MakeOwnershipAxiom(node)),
					MakeNodeContainsAxiom(MakeExpr(node), MakeDerefExpr(node, "val"))
				),
				MakeAxiomConjunction(
					MakeKeysetContainsAxiom(MakeExpr(node), MakeDerefExpr(node, "val"))
				)
			)
		);
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

	bool IsFlowDecreasing() const override {
		return true;
	}

	const Type& GetNodeType() const override {
		return nodeType;
	}

	const Predicate& GetOutFlowContains(std::string fieldname) const override {
		if (fieldname != "next") throw std::logic_error("KeysetFlow error: FlowDomain.GetOutFlowContains(std::string) expected 'next' but got '" + fieldname + "'.");
		return *outflowContains;
	}

	std::size_t GetFootprintDepth(const ConjunctionFormula& /*pre*/, const Dereference& lhs, const Expression& /*rhs*/) const override {
		// TODO important: if lhs.expr is owned, then a footprint of size 1 should do for pointer assignments?
		return lhs.sort() == Sort::PTR ? 2 : 0;
	}
};

std::unique_ptr<SolverImpl> plankton::MakeSolverImpl(const cola::Program& program) {
	auto& nodeType = extract_node_type(program);
	PostConfig config {
		std::make_unique<KeysetFlow>(nodeType, extract_outflow_predicate(program, nodeType)),
		extract_contains_predicate(program, nodeType),
		extract_invariant(program, nodeType)
	};
	return std::make_unique<SolverImpl>(std::move(config));
}
