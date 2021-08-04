#include "programs/util.hpp"

using namespace plankton;


struct RightMoveVisitor : public BaseProgramVisitor {
    bool isRightMover = true;
    void Visit(const TrueValue& /*node*/) override { /* do nothing */ }
    void Visit(const FalseValue& /*node*/) override { /* do nothing */ }
    void Visit(const NullValue& /*node*/) override { /* do nothing */ }
    void Visit(const MaxValue& /*node*/) override { /* do nothing */ }
    void Visit(const MinValue& /*node*/) override { /* do nothing */ }
    void Visit(const VariableExpression& node) override { isRightMover &= !node.Decl().isShared; }
    void Visit(const BinaryExpression& node) override { node.lhs->Accept(*this); node.rhs->Accept(*this); }
    void Visit(const Dereference& /*node*/) override { isRightMover = false; }
    void Visit(const Sequence& node) override { node.first->Accept(*this); node.second->Accept(*this); }
    void Visit(const Scope& node) override { node.body->Accept(*this); }
    void Visit(const Atomic& node) override { node.body->Accept(*this); }
    void Visit(const Choice& node) override { for (const auto& branch : node.branches) branch->Accept(*this); }
    void Visit(const UnconditionalLoop& node) override { node.body->Accept(*this); }
    void Visit(const Skip& /*node*/) override { /* do nothing */ }
    void Visit(const Break& /*node*/) override { /* do nothing */ }
    void Visit(const Assume& node) override { node.condition->Accept(*this); }
    void Visit(const Assert& node) override { node.condition->Accept(*this); }
    void Visit(const Return& node) override { for (const auto& expr : node.expressions) expr->Accept(*this); }
    void Visit(const Malloc& node) override { isRightMover &= !node.lhs.is_shared; }
    void Visit(const Macro& /*node*/) override { isRightMover = false; }
    template<typename T>
    void HandleAssignment(const T& node) {
    
    }
    void Visit(const MemoryRead& node) override { node.lhs->Accept(*this); node.rhs->Accept(*this); }
    void Visit(const MemoryWrite& node) override { for (const auto& lhs : node.lhs) lhs->Accept(*this); for (const auto& rhs : node.rhs) rhs->accept(*this); }
    void Visit(const Function& /*node*/) override { isRightMover = false; }
};

bool plankton::IsRightMover(const Statement& stmt) {
    RightMoveVisitor visitor;
    stmt.Accept(visitor);
    return visitor.isRightMover;
}