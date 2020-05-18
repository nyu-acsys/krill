#include "cola/transform.hpp"

#include "cola/ast.hpp"
#include "cola/util.hpp"

using namespace cola;


struct IsTrueConditionVisitor final : public BaseVisitor {
	bool result = false;
	void visit(const VariableDeclaration& /*node*/) override {}
	void visit(const Expression& /*node*/) override {}
	void visit(const BooleanValue& node) override { if (node.value) { result = true; } }
	void visit(const NullValue& /*node*/) override {}
	void visit(const EmptyValue& /*node*/) override {}
	void visit(const MaxValue& /*node*/) override {}
	void visit(const MinValue& /*node*/) override {}
	void visit(const NDetValue& /*node*/) override {}
	void visit(const VariableExpression& /*node*/) override {}
	void visit(const NegatedExpression& /*node*/) override {}
	void visit(const BinaryExpression& /*node*/) override {}
	void visit(const Dereference& /*node*/) override {}
	void visit(const CompareAndSwap& /*node*/) override {}
};

bool isTrueCondition(const Expression& expr) {
	IsTrueConditionVisitor visitor;
	expr.accept(visitor);
	return visitor.result;
}


struct RemoveConditionalsVisitor final : public BaseNonConstVisitor {
	std::unique_ptr<Statement> replacement;
	bool replacementNeeded = false;

	void visit(Program& program) {
		program.initializer->accept(*this);
		for (auto& func : program.functions) {
			func->accept(*this);
		}
	}

	void visit(Function& function) {
		if (function.body) {
			function.body->accept(*this);
		}
	}

	void visit(Atomic& atomic) {
		atomic.body->accept(*this);
	}

	void visit(Choice& choice) {
		for (auto& scope : choice.branches) {
			scope->accept(*this);
		}
	}
	void visit(Loop& loop) {
		loop.body->accept(*this);
	}

	void visit(IfThenElse& ite) {
		ite.ifBranch->accept(*this);
		ite.elseBranch->accept(*this);
	}

	std::unique_ptr<Statement> makeBreak(std::unique_ptr<Expression> expr) {
		return std::make_unique<IfThenElse>(cola::negate(*expr),
			std::make_unique<Scope>(std::make_unique<Break>()),
			std::make_unique<Scope>(std::make_unique<Skip>()));
	}

	void visit(While& whl) {
		whl.body->accept(*this);
		
		if (!isTrueCondition(*whl.expr)) {
			// whl.body->body = std::make_unique<Sequence>(makeBreak(std::move(whl.expr)), std::move(whl.body->body));
			// whl.expr = std::make_unique<BooleanValue>(true);

			replacementNeeded = true;
			auto postAssumption = std::make_unique<Assume>(cola::negate(*whl.expr));
			auto loopAssumption = std::make_unique<Assume>(std::move(whl.expr));
			auto body = std::make_unique<Sequence>(std::move(loopAssumption), std::move(whl.body->body));
			auto loop = std::make_unique<Loop>(std::make_unique<Scope>(std::move(body)));
			replacement = std::make_unique<Sequence>(std::move(loop), std::move(postAssumption));
		}
	}

	void visit(DoWhile& dwhl) {
		dwhl.body->accept(*this);
		// TODO: peel first loop iteration??
		
		replacementNeeded = true;
		if (isTrueCondition(*dwhl.expr)) {
			replacement = std::make_unique<While>(std::make_unique<BooleanValue>(true), std::move(dwhl.body));
		} else {
			auto body = std::make_unique<Sequence>(std::move(dwhl.body->body), makeBreak(std::move(dwhl.expr)));
			replacement = std::make_unique<DoWhile>(std::make_unique<BooleanValue>(true), std::make_unique<Scope>(std::move(body)));
		}
	}

	void acceptAndReplaceIfNeeded(std::unique_ptr<Statement>& uptr) {
		uptr->accept(*this);
		if (replacementNeeded) {
			assert(replacement);
			uptr = std::move(replacement);
			replacement.reset();
			replacementNeeded = false;
		} 
	}

	void visit(Scope& scope) {
		acceptAndReplaceIfNeeded(scope.body);
	}

	void visit(Sequence& sequence) {
		acceptAndReplaceIfNeeded(sequence.first);
		acceptAndReplaceIfNeeded(sequence.second);
	}

	void visit(Skip& /*node*/) { /* do nothing */ }
	void visit(Break& /*node*/) { /* do nothing */ }
	void visit(Continue& /*node*/) { /* do nothing */ }
	void visit(Assume& /*node*/) { /* do nothing */ }
	void visit(Assert& /*node*/) { /* do nothing */ }
	void visit(Return& /*node*/) { /* do nothing */ }
	void visit(Malloc& /*node*/) { /* do nothing */ }
	void visit(Assignment& /*node*/) { /* do nothing */ }
	void visit(Macro& /*node*/) { /* do nothing */ }
	void visit(CompareAndSwap& /*node*/) { /* do nothing */ }
};


void cola::remove_conditional_loops(Program& program) {
	RemoveConditionalsVisitor().visit(program);
}
