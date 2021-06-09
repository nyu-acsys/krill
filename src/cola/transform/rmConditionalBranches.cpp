#include "cola/transform.hpp"

#include "cola/ast.hpp"
#include "cola/util.hpp"

using namespace cola;


struct RemoveIfsVisitor final : public BaseNonConstVisitor {
    std::unique_ptr<Statement> replacement;
    bool replacementNeeded = false;

    void visit(Program& program) override {
        program.initializer->accept(*this);
        for (auto& func : program.functions) {
            func->accept(*this);
        }
    }

    void visit(Function& function) override {
        if (function.body) {
            function.body->accept(*this);
        }
    }

    void visit(Atomic& atomic) override {
        atomic.body->accept(*this);
    }

    void visit(Choice& choice) override {
        for (auto& scope : choice.branches) {
            scope->accept(*this);
        }
    }
    void visit(Loop& loop) override {
        loop.body->accept(*this);
    }

    void visit(IfThenElse& ite) override {
        ite.ifBranch->accept(*this);
        ite.elseBranch->accept(*this);

        replacementNeeded = true;
        ite.elseBranch->body = std::make_unique<Sequence>(std::make_unique<Assume>(cola::negate(*ite.expr)), std::move(ite.elseBranch->body));
        ite.ifBranch->body = std::make_unique<Sequence>(std::make_unique<Assume>(std::move(ite.expr)), std::move(ite.ifBranch->body));
        auto choice = std::make_unique<Choice>();
        choice->branches.push_back(std::move(ite.ifBranch));
        choice->branches.push_back(std::move(ite.elseBranch));
        replacement = std::move(choice);
    }

    void visit(While& whl) override {
        whl.body->accept(*this);
    }

    void visit(DoWhile& dwhl) override {
        dwhl.body->accept(*this);
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

    void visit(Scope& scope) override {
        acceptAndReplaceIfNeeded(scope.body);
    }

    void visit(Sequence& sequence) override {
        acceptAndReplaceIfNeeded(sequence.first);
        acceptAndReplaceIfNeeded(sequence.second);
    }

    void visit(Skip& /*node*/) override { /* do nothing */ }
    void visit(Break& /*node*/) override { /* do nothing */ }
    void visit(Continue& /*node*/) override { /* do nothing */ }
    void visit(Assume& /*node*/) override { /* do nothing */ }
    void visit(Assert& /*node*/) override { /* do nothing */ }
    void visit(Return& /*node*/) override { /* do nothing */ }
    void visit(Malloc& /*node*/) override { /* do nothing */ }
    void visit(Assignment& /*node*/) override { /* do nothing */ }
    void visit(Macro& /*node*/) override { /* do nothing */ }
    void visit(CompareAndSwap& /*node*/) override { /* do nothing */ }
};


void cola::remove_conditional_branching(Program& program) {
    RemoveIfsVisitor().visit(program);
}
