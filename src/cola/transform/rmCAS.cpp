#include "cola/transform.hpp"
#include "cola/util.hpp"
#include <set>

using namespace cola;

struct CasFinderVisitor : public BaseVisitor {
	bool containsCAS = false;
	bool negated = false;
	bool convertable = true;
	const CompareAndSwap* cas = nullptr;

	void visit(const NegatedExpression& node) {
		if (negated) convertable = false;
		negated = true;
		node.expr->accept(*this);
	}
	
	void visit(const CompareAndSwap& node) {
		if (containsCAS) convertable = false;
		containsCAS = true;
		cas = &node;
	}

	void visit(const BinaryExpression& /*node*/) { convertable = false; }
	void visit(const VariableDeclaration& /*node*/) { convertable = false; }
	void visit(const BooleanValue& /*node*/) { convertable = false; }
	void visit(const NullValue& /*node*/) { convertable = false; }
	void visit(const EmptyValue& /*node*/) { convertable = false; }
	void visit(const MaxValue& /*node*/) { convertable = false; }
	void visit(const MinValue& /*node*/) { convertable = false; }
	void visit(const NDetValue& /*node*/) { convertable = false; }
	void visit(const VariableExpression& /*node*/) { convertable = false; }
	void visit(const Dereference& /*node*/) { convertable = false; }
};

bool contains_cas(const Expression& expr) {
	CasFinderVisitor visitor;
	expr.accept(visitor);
	return visitor.containsCAS;
}

bool are_syntactically_equal(const Expression& expr, const Expression& other) {
	// TODO: implement a more robust solution
	std::stringstream exprStr, otherStr;
	cola::print(expr, exprStr);
	cola::print(other, otherStr);
	return exprStr.str() == otherStr.str();
}

std::unique_ptr<Expression> make_cas_condition(const CompareAndSwap& cas) {
	static const BinaryExpression::Operator opEQ = BinaryExpression::Operator::EQ;
	static const BinaryExpression::Operator opAND = BinaryExpression::Operator::AND;
	
	std::unique_ptr<Expression> result;
	for (const auto& elem : cas.elems) {
		auto cond = std::make_unique<BinaryExpression>(opEQ, cola::copy(*elem.dst), cola::copy(*elem.cmp));
		if (result) result = std::make_unique<BinaryExpression>(opAND, std::move(result), std::move(cond));
		else result = std::move(cond);
	}
	return result;
}

std::unique_ptr<Statement> make_cas_updates(const CompareAndSwap& cas) {
    auto result = std::make_unique<ParallelAssignment>();
    for (const auto& elem : cas.elems) {
        if (are_syntactically_equal(*elem.cmp, *elem.src)) continue;
        result->lhs.push_back(cola::copy(*elem.dst));
        result->rhs.push_back(cola::copy(*elem.src));
    }
    if (result->lhs.empty()) return std::make_unique<Skip>();
    return result;
}

std::tuple<bool, std::unique_ptr<Statement>, std::unique_ptr<Statement>> desugar_cas_expr(const Expression& expr, bool inside_atomic) {
	// returns <b, s1, s0>
	//      b:  indicates whether the given expr contains a CAS and needs rewriting
	//      s1: the true branch of the CAS desugared expr
	//      s0: the false branch of the CAS desugared expr

	CasFinderVisitor visitor;
	expr.accept(visitor);
	if (!visitor.containsCAS) {
		return std::make_tuple(false, nullptr, nullptr);
	}

	if (!visitor.convertable) {
		throw TransformationError("Cannot convert (nested) expression containing CAS.");
	}

	const CompareAndSwap& cas = *visitor.cas;
	if (cas.elems.size() == 0) {
		throw TransformationError("Cannot handle empty CAS.");
	}

	auto cond = make_cas_condition(cas);
	auto stmt = make_cas_updates(cas);

	std::unique_ptr<Statement> falseBranch = std::make_unique<Assume>(cola::negate(*cond));
	std::unique_ptr<Statement> trueBranch = std::make_unique<Sequence>(std::make_unique<Assume>(std::move(cond)), std::move(stmt));
	if (!inside_atomic) trueBranch = std::make_unique<Atomic>(std::make_unique<Scope>(std::move(trueBranch)));

	if (visitor.negated) return std::make_tuple(true, std::move(falseBranch), std::move(trueBranch));
	else return std::make_tuple(true, std::move(trueBranch), std::move(falseBranch));
}

void fail_if_cas(const Expression& expr, std::string info="") {
	if (contains_cas(expr)) {
		std::string verbose = info == "" ? "" : " from " + info;
		throw TransformationError("Cannot remove CAS" + verbose + ".");
	}
}

struct CasRemovalVisitor : public BaseNonConstVisitor {
	std::unique_ptr<Statement>* current_owner = nullptr;
	Function* current_function = nullptr;
	bool in_atomic = false;

	void visit(Skip& /*node*/) { /* do nothing */ }
	void visit(Break& /*node*/) { /* do nothing */ }
	void visit(Continue& /*node*/) { /* do nothing */ }
	void visit(Malloc& /*node*/) { /* do nothing */ }
	
	void handle_statement(std::unique_ptr<Statement>& stmt) {
		this->current_owner = &stmt;
		stmt->accept(*this);
	}

	void visit(Sequence& node) {
		handle_statement(node.first);
		handle_statement(node.second);
	}

	void visit(Scope& node) {
		handle_statement(node.body);
	}

	void visit(Atomic& node) {
		bool was_in_atomic = this->in_atomic;
		this->in_atomic = true;
		node.body->accept(*this);
		this->in_atomic = was_in_atomic;
	}

	void visit(Choice& node) {
		for (auto& branch : node.branches) {
			branch->accept(*this);
		}
	}

	void visit(IfThenElse& node) {
		std::unique_ptr<Statement>* my_owner = this->current_owner;
		node.ifBranch->accept(*this);
		node.elseBranch->accept(*this);
		this->current_owner = my_owner;

		auto [needs_rewriting, trueBranch, falseBranch] = desugar_cas_expr(*node.expr, this->in_atomic);
		if (needs_rewriting) {
			auto replacement = std::make_unique<Choice>();
			auto mkBranch = [](auto stmt1, auto stmt2) {
				return std::make_unique<Scope>(std::make_unique<Sequence>(std::move(stmt1), std::move(stmt2)));
			};
			replacement->branches.push_back(mkBranch(std::move(trueBranch), std::move(node.ifBranch->body)));
			replacement->branches.push_back(mkBranch(std::move(falseBranch), std::move(node.elseBranch->body)));
			*current_owner = std::move(replacement);
		}
	}

	void visit(Loop& node) {
		node.body->accept(*this);
	}

	void visit(While& node) {
		fail_if_cas(*node.expr, "while loop");
		node.body->accept(*this);
	}

	void visit(DoWhile& node) {
		fail_if_cas(*node.expr, "do-while loop");
		node.body->accept(*this);
	}

	void visit(Assume& node) {
		auto [needs_rewriting, trueBranch, falseBranch] = desugar_cas_expr(*node.expr, this->in_atomic);
		if (needs_rewriting) {
			assert(current_owner->get() == &node);
			*current_owner = std::move(trueBranch);
		}
	}

	void visit(Assert& node) {
		fail_if_cas(*node.expr, "assertion");
	}
	
	void visit(CompareAndSwap& node) {
		auto [needs_rewriting, trueBranch, falseBranch] = desugar_cas_expr(node, this->in_atomic);
		assert(needs_rewriting);
		if (needs_rewriting) {
			assert(current_owner->get() == &node);
			auto replacement = std::make_unique<Choice>();
			replacement->branches.push_back(std::make_unique<Scope>(std::move(trueBranch)));
			replacement->branches.push_back(std::make_unique<Scope>(std::move(falseBranch)));
			*current_owner = std::move(replacement);
		}
	}

	void visit(Return& node) {
		for (const auto& expr : node.expressions) {
			fail_if_cas(*expr, "return");
		}
	}

	void visit(Assignment& node) {
		fail_if_cas(*node.lhs, "assignment");
		// fail_if_cas(*node.rhs, "assignment");
		auto [needs_rewriting, trueBranch, falseBranch] = desugar_cas_expr(*node.rhs, this->in_atomic);
		if (needs_rewriting) {
			auto setTrue = std::make_unique<Assignment>(cola::copy(*node.lhs), std::make_unique<BooleanValue>(true));
			auto setFalse = std::make_unique<Assignment>(std::move(node.lhs), std::make_unique<BooleanValue>(false));
			trueBranch = std::make_unique<Sequence>(std::move(trueBranch), std::move(setTrue));
			falseBranch = std::make_unique<Sequence>(std::move(falseBranch), std::move(setFalse));
			auto replacement = std::make_unique<Choice>();
			replacement->branches.push_back(std::make_unique<Scope>(std::move(trueBranch)));
			replacement->branches.push_back(std::make_unique<Scope>(std::move(falseBranch)));
			*current_owner = std::move(replacement);
		}
	}
	void visit(Macro& node) {
		for (const auto& expr : node.args) {
			fail_if_cas(*expr, "return");
		}
	}
	
	void visit(Function& function) {
		if (function.body) {
			this->current_function = &function;
			function.body->accept(*this);
		}
	}

	void visit(Program& program) {
		program.initializer->accept(*this);
		for (auto& function : program.functions) {
			function->accept(*this);
		}
	}
};

void cola::remove_cas(Program& program) {
	CasRemovalVisitor visitor;
	program.accept(visitor);
}
