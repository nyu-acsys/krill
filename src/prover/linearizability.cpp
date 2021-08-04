#include "solver/verify.hpp"

#include "util/timer.hpp"
#include "solver/solver.hpp"

using namespace cola;
using namespace heal;
using namespace solver;
using namespace prover;


bool prover::CheckLinearizability(const Program& program, std::unique_ptr<Solver> solver) {
    Verifier verifier(program, std::move(solver));
    verifier.VerifyProgramOrFail();
    return true;
}

bool prover::CheckLinearizability(const Program& program, std::shared_ptr<SolverConfig> config) {
    return CheckLinearizability(program, solver::MakeDefaultSolver(std::move(config)));
}

Verifier::Verifier(const Program& program, std::unique_ptr<solver::Solver> solver_) : program(program), solver(std::move(solver_)), insideAtomic(false) {}

void Verifier::PerformStep(const std::function<std::unique_ptr<heal::Annotation>(std::unique_ptr<heal::Annotation>)>& transformer) {
    if (current.empty()) return;
    for (auto& annotation : current) {
        annotation = transformer(std::move(annotation));
    }
}

void Verifier::PerformStep(const std::function<solver::PostImage(std::unique_ptr<heal::Annotation>)>& transformer) {
    if (current.empty()) return;
    decltype(current) newCurrent;
    for (auto& annotation : current) {
        auto postImage = transformer(std::move(annotation));
        std::move(postImage.posts.begin(), postImage.posts.end(), std::back_inserter(newCurrent));
        AddNewInterference(std::move(postImage.effects));
    }
    current = std::move(newCurrent);
}

inline bool IsRightMover(const Statement& stmt) {
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

void Verifier::ApplyInterference(const Statement& after) {
    if (insideAtomic) return;
    if (current.empty()) return;
    if (interference.empty()) return;
    if (IsRightMover(after)) return;
    current = solver->MakeStable(std::move(current), interference);
//    PerformStep([this](auto annotation){
//        return engine->Interpolate(std::move(annotation), interference);
//    });
}

void Verifier::AddNewInterference(std::deque<std::unique_ptr<solver::HeapEffect>> effects) {
    std::move(effects.begin(), effects.end(), std::back_inserter(newInterference));
}

inline bool IsEffectEmpty(const std::unique_ptr<HeapEffect>& effect) {
    assert(effect->pre->node->Type() == effect->post->node->Type());
    if (&effect->pre->flow.get() != &effect->post->flow.get()) return false;
    for (const auto& [field, value] : effect->pre->fieldToValue) {
        if (&value->Decl() != &effect->post->fieldToValue.at(field)->Decl()) return false;
    }
    return true;
}

bool Verifier::ConsolidateNewInterference() {
    static Timer timer("Verifier::ConsolidateNewInterference");
    auto measurement = timer.Measure();

    // prune trivial noop interference
    // TODO: prune effects e which satisfy: e.post*e.context => e.pre*e.context ? They should not do anything
    newInterference.erase(std::remove_if(newInterference.begin(), newInterference.end(), IsEffectEmpty), newInterference.end());
    if (newInterference.empty()) return false;

    // rename to avoid name clashes
    SymbolicFactory factory;
    for (const auto& effect : interference) {
        factory.Avoid(*effect->pre);
        factory.Avoid(*effect->post);
        factory.Avoid(*effect->context);
    }
    for (auto& effect : newInterference) {
        heal::RenameSymbolicSymbols({ effect->pre.get(), effect->post.get(), effect->context.get() }, factory);
    }

    // debug
//    std::cout << std::endl << "Consolidating found effects: " << std::endl;
//    for (const auto& effect : newInterference) {
//        std::cout << "** effect: " << *effect->pre << " ~~> " << *effect->post << " under " << *effect->context << std::endl;
//    }
//    std::cout << std::endl << "Existing effects: " << std::endl;
//    for (const auto& effect : interference) {
//        std::cout << "** effect: " << *effect->pre << " ~~> " << *effect->post << " under " << *effect->context << std::endl;
//    }

    // generate all pairs of effects and check implication, order matters
    std::deque<std::pair<const HeapEffect*, const HeapEffect*>> pairs;
    for (auto& e : interference) for (auto& o : newInterference) pairs.emplace_back(e.get(), o.get());
    for (auto& e : newInterference) for (auto& o : interference) pairs.emplace_back(e.get(), o.get());
    for (auto& e : newInterference) for (auto& o : newInterference) if (e != o) pairs.emplace_back(e.get(), o.get());
    auto implications = solver->ComputeEffectImplications(pairs);

    // find unnecessary effects
    std::set<const HeapEffect*> prune;
    for (std::size_t index = 0; index < implications.size(); ++index) {
        auto [premise, conclusion] = pairs[index];
//        std::cout << " ++ implied=" << implications[index] << " contained=" << prune.count(premise) << " for: " << *premise->pre->node << " vs " << *conclusion->pre->node << std::endl;
        if (!implications[index]) continue;
        if (prune.count(premise) != 0) continue;
        prune.insert(conclusion);
    }

    // debug
//    std::cout << std::endl << "Pruning effects: " << std::endl;
//    for (const auto& effect : prune) {
//        std::cout << "** effect: " << *effect->pre << " ~~> " << *effect->post << " under " << *effect->context << std::endl;
//    }

    // remove unnecessary effects
    auto rm = [&prune](const auto& elem){ return prune.count(elem.get()) != 0; };
    interference.erase(std::remove_if(interference.begin(), interference.end(), rm), interference.end());
    newInterference.erase(std::remove_if(newInterference.begin(), newInterference.end(), rm), newInterference.end());

    // check if new effects exist
    if (newInterference.empty()) return false;

    // debug
    std::cout << std::endl << "New effects: " << std::endl;
    for (const auto& effect : newInterference) {
        std::cout << "** effect: " << *effect->pre << " ~~> " << *effect->post << " under " << *effect->context << std::endl;
    }

    // add new effects
    std::move(newInterference.begin(), newInterference.end(), std::back_inserter(interference));
    newInterference.clear();

    // debug
//    std::cout << std::endl << "Resulting overall effects: " << std::endl;
//    for (const auto& effect : interference) {
//        std::cout << "** effect: " << *effect->pre << " ~~> " << *effect->post << " under " << *effect->context << std::endl;
//    }

    return true;
}