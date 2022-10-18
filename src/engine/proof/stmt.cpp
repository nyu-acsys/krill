#include "engine/proof.hpp"

#include <sstream>
#include "logics/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;



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
    
    current = std::move(post);
}

void ProofGenerator::Visit(const UnconditionalLoop& stmt) {
    if (current.empty()) return;

    if (setup->loopJoinUntilFixpoint) {
        //
        // Join-based loop invariant search
        //

        // peel first loop iteration
        infoPrefix.Push("loop-", 0);
        INFO(infoPrefix << "Peeling first loop iteration..." << std::endl)
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

        auto joinCurrent = [this]() {
            JoinCurrent();
            assert(current.size() == 1);
            auto join = std::move(current.front());
            current.clear();
            return join;
        };

        // looping until fixed point
        if (!current.empty()) {
            std::size_t counter = 0;
            auto join = joinCurrent();
            while (true) {
                if (counter++ > setup->loopMaxIterations) throw std::logic_error("Aborting: loop does not seem to stabilize."); // TODO: remove / better error handling
                infoPrefix.Pop();
                infoPrefix.Push("loop-", counter);
                INFO(infoPrefix << "Starting iteration " << counter << " of loop invariant search..." << std::endl)
                DEBUG(std::endl << std::endl << " ------ loop " << counter << " ------ " << std::endl)

                breaking.clear();
                returning.clear();
                newInterference.clear();
                current.clear();
                current.push_back(plankton::Copy(*join));

                stmt.body->Accept(*this);
                current.push_back(plankton::Copy(*join)); // TODO: is this needed??
                auto newJoin = joinCurrent();

                INFO(infoPrefix << "Checking for loop invariant" << std::endl)
                if (solver.Implies(*newJoin, *join)) break;
                join = std::move(newJoin);
            }
        }

        INFO(infoPrefix << "Loop invariant found." << std::endl)
        infoPrefix.Pop();

        // post loop
        current = std::move(firstBreaking);
        MoveInto(std::move(breaking), current);
        LeaveAllNestedScopes(stmt);
        // PruneCurrent();
        // ImproveCurrentTime();
        // ReduceCurrentTime();
        if (setup->loopJoinPost) JoinCurrent();

        breaking = std::move(breakingOuter);
        MoveInto(std::move(returningOuter), returning);
        MoveInto(std::move(newInterferenceOuter), newInterference);


    } else {
        //
        // Widening-based loop invariant search
        //
        auto breakingOuter = std::move(breaking);
        breaking.clear();

        std::size_t counter = 0;
        infoPrefix.Push(""); // dummy, gets popped immediately
        decltype(current) posted;
        do {
            if (counter++ > setup->loopMaxIterations) throw std::logic_error("Aborting: widening does not seem to produce loop invariant."); // TODO: remove / better error handling
            infoPrefix.Pop();
            infoPrefix.Push("loop-", counter);
            INFO(infoPrefix << "Starting iteration " << counter << " of loop invariant search..." << std::endl)
            DEBUG(std::endl << std::endl << " ------ loop " << counter << " ------ " << std::endl)

            stmt.body->Accept(*this);
            ImproveCurrentTime();
            ReduceCurrentTime();
            ApplyTransformer([this](auto annotation) { return solver.Widen(std::move(annotation)); });
            PruneCurrent();

            plankton::RemoveIf(current, [this, &posted](const auto& post) {
                return plankton::Any(posted, [this, &post](const auto& pre) {
                    return solver.Implies(*post, *pre);
                });
            });
            plankton::MoveInto(plankton::CopyAll(current), posted);

        } while (!current.empty());

        current = std::move(breaking);
        LeaveAllNestedScopes(stmt);
        breaking = std::move(breakingOuter);

        INFO(infoPrefix << "Loop invariant found. (" << current.size() << ")" << std::endl)
        infoPrefix.Pop();

        ApplyTransformer([this](auto annotation) { return solver.Widen(std::move(annotation)); });
        PruneCurrent();
        ImproveCurrentTime();
        ReduceCurrentTime();
        PruneReturning();
    }
}
