#include "cola/transform.hpp"

using namespace cola;

static inline BinaryExpression::Operator negate_binary_opereator(BinaryExpression::Operator op) { // TODO: code copy -> negExpr.cpp
    switch (op) {
        case BinaryExpression::Operator::EQ: return BinaryExpression::Operator::NEQ;
        case BinaryExpression::Operator::NEQ: return BinaryExpression::Operator::EQ;
        case BinaryExpression::Operator::LEQ: return BinaryExpression::Operator::GT;
        case BinaryExpression::Operator::LT: return BinaryExpression::Operator::GEQ;
        case BinaryExpression::Operator::GEQ: return BinaryExpression::Operator::LT;
        case BinaryExpression::Operator::GT: return BinaryExpression::Operator::LEQ;
        case BinaryExpression::Operator::AND: return BinaryExpression::Operator::OR;
        case BinaryExpression::Operator::OR: return BinaryExpression::Operator::AND;
    }
}

struct ExpressionSimplifier : public BaseNonConstVisitor {
    bool negate = false;
    bool standalone = true;
    std::unique_ptr<Expression> result;

    void handle(std::unique_ptr<Expression>& expr) {
        auto old = std::move(result);
        result = std::move(expr);
        result->accept(*this);
        expr = std::move(result);
        result = std::move(old);
    }

    void visit(VariableExpression& expr) override {
        if (standalone && expr.sort() == Sort::BOOL) {
            result = std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, std::move(result), std::make_unique<BooleanValue>(!negate));
        }
    }
    void visit(Dereference& expr) override {
        if (standalone && expr.sort() == Sort::BOOL) {
            result = std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, std::move(result), std::make_unique<BooleanValue>(!negate));
        }
    }

    void visit(NegatedExpression& expr) override {
        bool oldNegate = negate;
        negate = !negate;
        handle(expr.expr);
        if (oldNegate) result = std::move(expr.expr);
        negate = oldNegate;
    }
    void visit(BinaryExpression& expr) override {
        bool oldNegate = negate;
        bool oldStandalone = standalone;
        if (negate) {
            negate = false;
            expr.op = negate_binary_opereator(expr.op);
        }
        standalone = expr.op == BinaryExpression::Operator::AND || expr.op == BinaryExpression::Operator::OR;
        assert(expr.rhs);
        handle(expr.lhs);
        assert(expr.rhs);
        handle(expr.rhs);
        negate = oldNegate;
        standalone = oldStandalone;
    }
    void visit(BooleanValue& expr) override {
        if (negate) expr.value = !expr.value;
    }

    void visit(VariableDeclaration& /*expr*/) override { /* do nothing */ }
    void visit(NullValue& /*expr*/) override { /* do nothing */ }
    void visit(EmptyValue& /*expr*/) override { /* do nothing */ }
    void visit(MaxValue& /*expr*/) override { /* do nothing */ }
    void visit(MinValue& /*expr*/) override { /* do nothing */ }
    void visit(NDetValue& /*expr*/) override { /* do nothing */ }
};

void simplify_expression(std::unique_ptr<Expression>& expression) {
    static ExpressionSimplifier simplifier;
    simplifier.handle(expression);
}

struct ExpressionVisitor : public BaseNonConstVisitor {
    void visit(Program& program) override { for (auto& func : program.functions) func->accept(*this); }
    void visit(Function& function) override { function.body->accept(*this); }
    void visit(Scope& scope) override { scope.body->accept(*this); }
    void visit(Sequence& sequence) override { sequence.first->accept(*this); sequence.second->accept(*this); }
    void visit(Atomic& atomic) override { atomic.body->accept(*this); }
    void visit(Choice& choice) override { for (auto& scope : choice.branches) scope->accept(*this); }
    void visit(IfThenElse& ite) override { ite.ifBranch->accept(*this); ite.elseBranch->accept(*this); simplify_expression(ite.expr); }
    void visit(Loop& loop) override { loop.body->accept(*this); }
    void visit(While& whl) override { whl.body->accept(*this); simplify_expression(whl.expr); }
    void visit(DoWhile& whl) override { whl.body->accept(*this); simplify_expression(whl.expr); }
    void visit(Assume& cmd) override { simplify_expression(cmd.expr); }
    void visit(Assert& cmd) override { simplify_expression(cmd.expr); }
    void visit(Skip& /*node*/) override { /* do nothing */ }
    void visit(Break& /*node*/) override { /* do nothing */ }
    void visit(Continue& /*node*/) override { /* do nothing */ }
    void visit(Return& /*node*/) override { /* do nothing */ }
    void visit(Malloc& /*node*/) override { /* do nothing */ }
    void visit(Assignment& /*node*/) override { /* do nothing */ }
    void visit(Macro& /*node*/) override { /* do nothing */ }
    void visit(CompareAndSwap& /*node*/) override { /* do nothing */ }
};

void cola::simplify_conditions(Program& program) {
    ExpressionVisitor().visit(program);
}

