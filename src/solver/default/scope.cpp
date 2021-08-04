#include "engine/solver.hpp"

#include "heal/util.hpp"
#include "post_helper.hpp"

using namespace cola;
using namespace heal;
using namespace engine;


inline std::unique_ptr<heal::Annotation> AddScope(std::unique_ptr<Annotation> pre, const std::vector<std::unique_ptr<VariableDeclaration>>& scope) {
    SymbolicFactory factory(*pre);
    for (const auto& variable : scope) {
        if (FindResource(*variable, *pre)) throw std::logic_error("Variable hiding is not supported."); // TODO: better error handling
        pre->now->conjuncts.push_back(std::make_unique<EqualsToAxiom>(
                std::make_unique<VariableExpression>(*variable),
                std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(variable->type))
        ));
    }
    return pre;
}

inline std::unique_ptr<heal::Annotation> RemoveScope(std::unique_ptr<Annotation> pre, const std::vector<std::unique_ptr<VariableDeclaration>>& scope) {
    if (scope.empty()) return pre;

    // remove (assumes all resources on top level)
    heal::Simplify(*pre);
    auto needsRemoval = [&scope](const Formula& formula){
        if (auto variable = dynamic_cast<const EqualsToAxiom*>(&formula)) {
            auto find = std::find_if(scope.begin(), scope.end(), [&variable](const auto& elem){ return elem.get() == &variable->variable->decl; });
            return find != scope.end();
        }
        return false;
    };
    for (auto it = pre->now->conjuncts.begin(); it != pre->now->conjuncts.end();) {
        if (needsRemoval(**it)) it = pre->now->conjuncts.erase(it);
        else ++it;
    }

    // check removal
    struct Check : public DefaultLogicListener {
        const std::function<bool(const Formula&)>& needsRemoval;
        explicit Check(const std::function<bool(const Formula&)>& needsRemoval) : needsRemoval(needsRemoval) {}
        void enter(const EqualsToAxiom& object) override {
            if (!needsRemoval(object)) return;
            throw std::logic_error("Could not remove resources for leaving scope."); // TODO: better error handling
        }
    } checker(needsRemoval);
    pre->accept(checker);

    return pre;
}

std::unique_ptr<heal::Annotation> DefaultSolver::PostEnterScope(std::unique_ptr<Annotation> pre, const Program& scope) const {
    return AddScope(std::move(pre), scope.variables);
}

std::unique_ptr<heal::Annotation> DefaultSolver::PostEnterScope(std::unique_ptr<Annotation> pre, const Function& scope) const {
    return AddScope(std::move(pre), scope.args);
}

std::unique_ptr<heal::Annotation> DefaultSolver::PostEnterScope(std::unique_ptr<Annotation> pre, const Scope& scope) const {
    return AddScope(std::move(pre), scope.variables);
}

std::unique_ptr<heal::Annotation> DefaultSolver::PostLeaveScope(std::unique_ptr<Annotation> pre, const Program& scope) const {
    throw std::logic_error("Cannot leave program scope."); // TODO: better error handling
}

inline std::set<const Scope*> CollectAllScopes(const Function& func) {
    struct : public BaseVisitor {
        std::set<const Scope*> result;
        void visit(const Sequence& node) override { node.first->accept(*this); node.second->accept(*this); }
        void visit(const Scope& node) override { result.insert(&node); node.body->accept(*this); }
        void visit(const Atomic& node) override { node.body->accept(*this); }
        void visit(const Choice& node) override { for (auto& elem : node.branches) elem->accept(*this); }
        void visit(const IfThenElse& node) override { node.ifBranch->accept(*this); node.elseBranch->accept(*this); }
        void visit(const Loop& node) override { node.body->accept(*this); }
        void visit(const While& node) override { node.body->accept(*this); }
        void visit(const DoWhile& node) override { node.body->accept(*this); }
        void visit(const Function& node) override { node.body->accept(*this); }

        void visit(const Skip& /*node*/) override { /* do nothing */ }
        void visit(const Break& /*node*/) override { /* do nothing */ }
        void visit(const Continue& /*node*/) override { /* do nothing */ }
        void visit(const Assume& /*node*/) override { /* do nothing */ }
        void visit(const Assert& /*node*/) override { /* do nothing */ }
        void visit(const Return& /*node*/) override { /* do nothing */ }
        void visit(const Malloc& /*node*/) override { /* do nothing */ }
        void visit(const Assignment& /*node*/) override { /* do nothing */ }
        void visit(const ParallelAssignment& /*node*/) override { /* do nothing */ }
        void visit(const Macro& /*node*/) override { /* do nothing */ }
        void visit(const CompareAndSwap& /*node*/) override { /* do nothing */ }
    } collector;
    func.accept(collector);
    return std::move(collector.result);
}

std::unique_ptr<heal::Annotation> DefaultSolver::PostLeaveScope(std::unique_ptr<Annotation> pre, const Function& scope) const {
    auto subScopes = CollectAllScopes(scope);
    for (const auto* subScope : subScopes) pre = RemoveScope(std::move(pre), subScope->variables);
    return RemoveScope(std::move(pre), scope.args);
}

std::unique_ptr<heal::Annotation> DefaultSolver::PostLeaveScope(std::unique_ptr<Annotation> pre, const Scope& scope) const {
    return RemoveScope(std::move(pre), scope.variables);
}
