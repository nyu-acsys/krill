#include "default_solver.hpp"

#include "cola/visitors.hpp"
#include "info.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


inline std::unique_ptr<Annotation> MakePost(PostInfo info, const Expression& lhs, const Expression& rhs) {
    auto [isLhsVar, lhsVar] = heal::is_of_type<VariableExpression>(lhs);
    if (isLhsVar) return MakeVarAssignPost(std::move(info), { std::make_pair(std::cref(*lhsVar), std::cref(rhs)) });

    auto [isLhsDeref, lhsDeref] = heal::is_of_type<Dereference>(lhs);
    if (!isLhsDeref) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a variable or a dereference");

    auto [isDerefVar, derefVar] = heal::is_of_type<VariableExpression>(*lhsDeref->expr);
    if (!isDerefVar) ThrowUnsupportedAssignmentError(lhs, rhs, "expected left-hand side to be a dereference of a variable");

    // ensure rhs is a 'SimpleExpression' or the negation of such
    auto [isRhsNegation, rhsNegation] = heal::is_of_type<NegatedExpression>(rhs);
    auto [isSimple, rhsSimple] = heal::is_of_type<SimpleExpression>(isRhsNegation ? *rhsNegation->expr : rhs);
    if (!isSimple) ThrowUnsupportedAssignmentError(lhs, rhs, "expected right-hand side to be a variable or an immediate value");

    return MakeDerefAssignPost(std::move(info), *lhsDeref, rhs);
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, parallel_assignment_t assignments) const {
    // ensure that no variable is assigned multiple times
    for (std::size_t index = 0; index < assignments.size(); ++index) {
        for (std::size_t other = index+1; other < assignments.size(); ++other) {
            if (heal::syntactically_equal(assignments[index].first, assignments[index].second)) {
                throw UnsupportedConstructError("Malformed parallel assignment: variable " + assignments[index].first.get().decl.name + " is assigned multiple times.");
            }
        }
    }

    // compute post
    return prover::PostProcess(MakeVarAssignPost(PostInfo(*this, pre), assignments), pre);
}

std::unique_ptr<Annotation> SolverImpl::Post(const Annotation& pre, const Assignment& cmd) const {
    return prover::PostProcess(MakePost(PostInfo(*this, pre), *cmd.lhs, *cmd.rhs), pre);
}

bool SolverImpl::UncheckedPostEntails(const ConjunctionFormula& pre, const Assignment& cmd, const ConjunctionFormula& post) const {
    auto partialPostImage = MakePost(PostInfo(*this, pre, post), *cmd.lhs, *cmd.rhs);
    return partialPostImage->now->conjuncts.size() == post.conjuncts.size();
}

struct SimplifiedExpression {
    std::optional<const SimpleExpression*> simple;
    std::optional<std::tuple<const Dereference*, const VariableExpression*>> deref;

    explicit SimplifiedExpression() = default;
    explicit SimplifiedExpression(const SimpleExpression& expr) : simple(&expr) {}
    explicit SimplifiedExpression(const Dereference& deref, const VariableExpression& var) : deref({ &deref, &var }) {}
};

SimplifiedExpression Unravel(const Expression& expr) {
    auto [isSimple, simple] = heal::IsOfType<SimpleExpression>(expr);
    if (isSimple) return SimplifiedExpression(*simple);

    auto [isDeref, deref] = heal::IsOfType<Dereference>(expr);
    if (!isDeref) return SimplifiedExpression();

    auto [isVar, var] = heal::IsOfType<VariableExpression>(deref->expr);
    if (isVar) return SimplifiedExpression(*deref, *var);
    return SimplifiedExpression();
}

struct PostVisitor : public BaseVisitor {
    PostInfo info;
    const Assignment& cmd;
    std::unique_ptr<Annotation> result;

    PostVisitor(PostInfo info, const Assignment& cmd) : info(std::move(info)), cmd(cmd) {
        cmd.lhs->accept(*this);
    }

    void visit(const VariableExpression& var) override {
        auto rhs = Unravel(*cmd.rhs);
        if (rhs.simple) {
            result = MakePost(std::move(info), var, **rhs.simple);
        } else if (rhs.deref) {
            auto [deref, var] = *rhs.deref;
            result = MakePost(std::move(info), *deref, *var);
        } else {
            throw std::logic_error("unsupported assignment"); // TODO: better error class
        }
    }

    void visit(const Dereference& node) override {
        auto rhs = Unravel(*cmd.rhs);
        if (rhs.deref) {
            auto [deref, var] = *rhs.deref;
            result = MakePost(std::move(info), *deref, *var);
        } else {
            throw std::logic_error("unsupported assignment"); // TODO: better error class
        }
    }

    static std::unique_ptr<Annotation> MakePost(PostInfo info, const Assignment& cmd) {
        PostVisitor visitor(info, cmd);
        return std::move(visitor.result);
    }
};


std::unique_ptr<Annotation> DefaultSolver::Post(const Annotation &pre, const Assignment &cmd) const {
    return PostVisitor::MakePost(PostInfo(*this, pre), cmd);
}

bool DefaultSolver::PostEntailsUnchecked(const Formula& pre, const Assignment& cmd, const Formula& post) const {
    throw std::logic_error("not yet implemented: PostEntailsUnchecked");
}
