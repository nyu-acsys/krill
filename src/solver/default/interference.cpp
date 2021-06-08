#include "default_solver.hpp"

#include "heal/collect.hpp"
#include "encoder.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


void HandleInterference(ImplicationCheckSet& checks, Z3Encoder<>& encoder, const Annotation& annotation, PointsToAxiom& memory, const HeapEffect& effect) {
    // TODO: avoid encoding the same annotation/effect multiple times
    if (memory.node->Type() != effect.pre->node->Type()) return;
    if (&effect.pre->node->Decl() != &effect.post->node->Decl()) throw std::logic_error("Unsupported effect"); // TODO: better error handling

    auto interference = encoder(annotation) && encoder(*effect.context) && encoder.MakeMemoryEquality(memory, *effect.pre);
    auto isInterferenceFree = z3::implies(interference, encoder.context.bool_val(false));
    checks.Add(isInterferenceFree, [&memory,&effect](bool holds){
        if (holds) return;
        memory.flow = effect.post->flow;
        for (auto& [field, value] : memory.fieldToValue) {
            value = heal::Copy(*effect.post->fieldToValue.at(field));
        }
    });
}

std::deque<std::unique_ptr<heal::Annotation>> DefaultSolver::MakeStable(std::deque<std::unique_ptr<heal::Annotation>> annotations,
                                                                        const std::deque<std::unique_ptr<HeapEffect>>& interferences) const {
    // make annotation symbols distinct from interference symbols
    SymbolicFactory factory;
    for (const auto& effect : interferences) {
        factory.Avoid(*effect->pre);
        factory.Avoid(*effect->post);
        factory.Avoid(*effect->context);
    }
    for (auto& annotation : annotations) {
        heal::RenameSymbolicSymbols(*annotation, factory);
    }

    // prepare solving
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    ImplicationCheckSet checks(context);

    // apply interference
    for (auto& annotation : annotations) {
        auto resources = heal::CollectMemory(*annotation->now);
        for (auto* memory : resources) {
            if (memory->isLocal) continue;
            auto nonConstMemory = const_cast<PointsToAxiom*>(memory); // TODO: does this work?
            for (const auto& effect : interferences) {
                HandleInterference(checks, encoder, *annotation, *nonConstMemory, *effect);
            }
        }
    }

    solver::ComputeImpliedCallback(solver, checks);
    return annotations;
}
