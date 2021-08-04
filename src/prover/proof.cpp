#include "engine/proof.hpp"

#include "util/shortcuts.hpp"

using namespace cola;
using namespace heal;
using namespace engine;


ProofGenerator::ProofGenerator(const Program& program, const SolverConfig& config)
        : program(program), solver(config), insideAtomic(false) {}

void ProofGenerator::visit(const cola::Function& func) {
    func.body->accept(*this);
    assert(breaking.empty());
    for (auto& annotation : current) {
        returning.emplace_back(std::move(annotation), nullptr);
    }
    current.clear();
}

void ProofGenerator::ApplyTransformer(const std::function<std::unique_ptr<heal::Annotation>(std::unique_ptr<heal::Annotation>)>& transformer) {
    if (current.empty()) return;
    for (auto& annotation : current) {
        annotation = transformer(std::move(annotation));
    }
}

void ProofGenerator::ApplyTransformer(const std::function<PostImage(std::unique_ptr<heal::Annotation>)>& transformer) {
    if (current.empty()) return;
    decltype(current) newCurrent;
    for (auto& annotation : current) {
        auto postImage = transformer(std::move(annotation));
        util::MoveInto(std::move(postImage.annotations), newCurrent);
        AddNewInterference(std::move(postImage.effects));
    }
    current = std::move(newCurrent);
}

void ProofGenerator::AddNewInterference(std::deque<std::unique_ptr<HeapEffect>> effects) {
    util::MoveInto(effects, newInterference);
}

bool ProofGenerator::ConsolidateNewInterference() {
    auto result = solver.AddInterference(std::move(newInterference));
    newInterference.clear();
    return result;
}

inline bool IsRightMover(const Statement& stmt) { // TODO: move into program util.hpp
    struct : public BaseVisitor {
        bool isRightMover = true;
        void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
        void visit(const NullValue& /*node*/) override { /* do nothing */ }
        void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
        void visit(const MaxValue& /*node*/) override { /* do nothing */ }
        void visit(const MinValue& /*node*/) override { /* do nothing */ }
        void visit(const NDetValue& /*node*/) override { /* do nothing */ }
        void visit(const VariableExpression& node) override { isRightMover &= !node.decl.is_shared; }
        void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
        void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
        void visit(const Dereference& /*node*/) override { isRightMover = false; }
        void visit(const Sequence& node) override { node.first->accept(*this); node.second->accept(*this); }
        void visit(const Scope& node) override { node.body->accept(*this); }
        void visit(const Atomic& node) override { node.body->accept(*this); }
        void visit(const Choice& node) override { for (const auto& branch : node.branches) branch->accept(*this); }
        void visit(const IfThenElse& node) override { node.expr->accept(*this); node.ifBranch->accept(*this); node.elseBranch->accept(*this); }
        void visit(const Loop& node) override { node.body->accept(*this); }
        void visit(const While& node) override { node.expr->accept(*this); node.body->accept(*this); }
        void visit(const DoWhile& node) override { node.expr->accept(*this); node.body->accept(*this); }
        void visit(const Skip& /*node*/) override { /* do nothing */ }
        void visit(const Break& /*node*/) override { /* do nothing */ }
        void visit(const Continue& /*node*/) override { /* do nothing */ }
        void visit(const Assume& node) override { node.expr->accept(*this); }
        void visit(const Assert& node) override { node.expr->accept(*this); }
        void visit(const Return& node) override { for (const auto& expr : node.expressions) expr->accept(*this); }
        void visit(const Malloc& node) override { isRightMover &= !node.lhs.is_shared; }
        void visit(const Assignment& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
        void visit(const ParallelAssignment& node) override { for (const auto& lhs : node.lhs) lhs->accept(*this); for (const auto& rhs : node.rhs) rhs->accept(*this); }
        void visit(const Macro& /*node*/) override { isRightMover = false; }
        void visit(const CompareAndSwap& /*node*/) override { isRightMover = false; }
    } checker;
    stmt.accept(checker);
    if (checker.isRightMover) std::cout << "! no interference needed for right mover: " << stmt << std::endl;
    return checker.isRightMover;
}

void ProofGenerator::MakeInterferenceStable(const Statement& after) {
    if (insideAtomic) return;
    if (current.empty()) return;
    if (IsRightMover(after)) return;
    ApplyTransformer([this](auto annotation){
        return solver.MakeInterferenceStable(std::move(annotation));
    });
}
