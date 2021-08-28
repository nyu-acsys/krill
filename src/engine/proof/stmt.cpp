#include "engine/proof.hpp"

#include <sstream>
#include "logics/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

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

void ProofGenerator::Visit(const Choice& stmt) {
    if (stmt.branches.empty()) return;
    
    auto pre = std::move(current);
    decltype(pre) post;
    current.clear();
    
    for (const auto& branch : stmt.branches) {
        current = plankton::CopyAll(pre);
        branch->Accept(*this);
        MoveInto(std::move(current), post);
    }
}

void ProofGenerator::Visit(const UnconditionalLoop& stmt) {
    if (current.empty()) return;
    
    // peel first loop iteration
    DEBUG(std::endl << " ------ loop 0 (peeled) ------ " << std::endl)
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
        auto joinCurrent = [this]() {
            auto join = solver.Join(std::move(current));
            join = plankton::Normalize(std::move(join));
            current.clear();
            return join;
        };

        std::size_t counter = 0;
        auto join = joinCurrent();
        while (true) {
            if (counter++ > LOOP_ABORT_AFTER) throw std::logic_error("Aborting: loop does not seem to stabilize."); // TODO: remove / better error handling
            DEBUG(std::endl << std::endl << " ------ loop " << counter << " ------ " << std::endl)
            
            breaking.clear();
            returning.clear();
            newInterference.clear();
            current.clear();
            current.push_back(plankton::Copy(*join));
            
            stmt.body->Accept(*this);
            current.push_back(plankton::Copy(*join)); // TODO: is this needed??
            auto newJoin = joinCurrent();

            if (plankton::SyntacticalEqual(*newJoin, *join)) break;
            join = std::move(newJoin);
        }
    }
    
    // post loop
    current = std::move(firstBreaking);
    MoveInto(std::move(breaking), current);
    breaking = std::move(breakingOuter);
    MoveInto(std::move(returningOuter), returning);
    MoveInto(std::move(newInterferenceOuter), newInterference);
}
