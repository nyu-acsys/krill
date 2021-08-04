#include "engine/proof.hpp"

#include <set>
#include <sstream>
#include "util/timer.hpp"
//#include "cola/util.hpp"
#include "logics/util.hpp"
//#include "util/logger.hpp" // TODO:remove
//#include "heal/collect.hpp"

using namespace plankton;


constexpr std::size_t LOOP_ABORT_AFTER = 7;


void ProofGenerator::Visit(const Sequence& stmt) {
    stmt.first->Accept(*this);
    stmt.second->Accept(*this);
}

void ProofGenerator::Visit(const Scope& stmt) {
    ApplyTransformer([this,&stmt](auto annotation){ return solver.PostEnter(std::move(annotation), stmt); });
    stmt.body->Accept(*this);
    ApplyTransformer([this,&stmt](auto annotation){ return solver.PostLeave(std::move(annotation), stmt); });
}

void ProofGenerator::Visit(const Atomic& stmt) {
    bool old_inside_atomic = insideAtomic;
    insideAtomic = true;
    stmt.body->Accept(*this);
    insideAtomic = old_inside_atomic;
    MakeInterferenceStable(stmt);
}

inline std::deque<std::unique_ptr<Annotation>> CopyList(const std::deque<std::unique_ptr<Annotation>>& list) {
    std::deque<std::unique_ptr<Annotation>> result;
    for (const auto& elem : list) result.push_back(plankton::Copy(*elem));
    return result;
}

void ProofGenerator::Visit(const Choice& stmt) {
    if (stmt.branches.empty()) return;
    
    auto pre = std::move(current);
    decltype(pre) post;
    current.clear();
    
    // handle all but first branch => current annotations need to be copied
    for (auto it = std::next(stmt.branches.begin()); it != stmt.branches.end(); ++it) {
        current = CopyList(pre);
        it->get()->Accept(*this);
        std::move(current.begin(), current.end(), std::back_inserter(post));
    }
    
    // for the remaining first branch elide the copy
    current = std::move(pre);
    stmt.branches.front()->Accept(*this);
    std::move(post.begin(), post.end(), std::back_inserter(current));
}

void ProofGenerator::visit(const UnconditionalLoop& stmt) {
    assert(currentFunction);
    if (current.empty()) return;
    
    // peel first loop iteration
    std::cout << std::endl << std::endl << " ------ loop 0 (peeled) ------ " << std::endl;
    auto breakingOuter = std::move(breaking);
    breaking.clear();
    stmt.body->Accept(*this);
    auto firstBreaking = std::move(breaking);
    auto returningOuter = std::move(returning);
    auto newInterferenceOuter = std::move(newInterference);
    breaking.clear();
    returning.clear();
    newInterference.clear();
    
    // looping until fixed point
    if (!current.empty()) {
        auto PrepareJoin = [this](){
            if (current.size() <= 1) return;
            PerformStep([this](auto annotation){
                return solver->TryFindBetterHistories(std::move(annotation), interference);
            });
        };
        std::size_t counter = 0;
        PrepareJoin();
        auto join = solver->Join(std::move(current));
        while (true) {
            if (counter > LOOP_ABORT_AFTER) throw std::logic_error("Aborting: loop does not seem to stabilize."); // TODO: remove / better error handling
            std::cout << std::endl << std::endl << " ------ loop " << ++counter << " ------ " << std::endl;
            
            breaking.clear();
            returning.clear();
            newInterference.clear();
            current.clear();
            current.push_back(plankton::Copy(*join));
            
            stmt.body->Accept(*this);
            current.push_back(plankton::Copy(*join));
            PrepareJoin();
            auto newJoin = solver->Join(std::move(current));
            if (solver->Implies(*newJoin, *join) == Solver::YES) break;
            join = std::move(newJoin);
        }
    }
    
    // post loop
    current = std::move(firstBreaking);
    std::move(breaking.begin(), breaking.end(), std::back_inserter(current));
    breaking = std::move(breakingOuter);
    std::move(returningOuter.begin(), returningOuter.end(), std::back_inserter(returning));
    std::move(newInterferenceOuter.begin(), newInterferenceOuter.end(), std::back_inserter(newInterference));
}
