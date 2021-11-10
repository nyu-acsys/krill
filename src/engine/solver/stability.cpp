#include "engine/solver.hpp"

#include <map>
#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/util.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;


struct InterferenceInfo {
    SymbolFactory factory;
    Encoding encoding;
    std::unique_ptr<Annotation> annotation;
    const std::deque<std::unique_ptr<HeapEffect>>& interference;
    std::map<const SharedMemoryCore*, std::deque<std::function<void()>>> stabilityUpdates;

    explicit InterferenceInfo(std::unique_ptr<Annotation> annotation_, const std::deque<std::unique_ptr<HeapEffect>>& interference)
            : annotation(std::move(annotation_)), interference(interference) {
        assert(annotation);
        Preprocess();
        Compute();
        Apply();
        Postprocess();
    }

    inline void Preprocess() {
        // make symbols distinct
        plankton::AvoidEffectSymbols(factory, interference);
        plankton::RenameSymbols(*annotation, factory);

        // DEBUG("<<INTERFERENCE>>" << std::endl)
        // for (const auto& effect : interference) DEBUG("  -- effect: " << *effect << std::endl)
        // DEBUG(" -- pre: " << *annotation << std::endl;)
    }

    inline void Handle(SharedMemoryCore& memory, const HeapEffect& effect) {
        // TODO: avoid encoding the same annotation/effect multiple times
        if (memory.node->Type() != effect.pre->node->Type()) return;
        if (&effect.pre->node->Decl() != &effect.post->node->Decl()) throw std::logic_error("Unsupported effect"); // TODO: better error handling
    
        auto effectMatch = encoding.EncodeMemoryEquality(memory, *effect.pre) && encoding.Encode(*effect.context);
        auto isInterferenceFree = effectMatch >> encoding.Bool(false);
        auto applyEffect = [this, &memory, &effect]() {
            if (UpdatesFlow(effect)) memory.flow->decl = factory.GetFreshSO(memory.flow->Type());
            for (auto&[field, value] : memory.fieldToValue) {
                if (!UpdatesField(effect, field)) continue;
                value->decl = factory.GetFreshFO(value->Type());
            }
        };
        encoding.AddCheck(isInterferenceFree, [this, &memory, applyEffect = std::move(applyEffect)](bool isStable) {
            if (isStable) return;
            stabilityUpdates[&memory].push_back(std::move(applyEffect));
        });
    }

    inline void Compute() {
        encoding.AddPremise(*annotation->now);
        auto resources = plankton::CollectMutable<SharedMemoryCore>(*annotation->now);
        for (auto* memory : resources) {
            for (const auto& effect : interference) {
                Handle(*memory, *effect);
            }
        }
        encoding.Check();
    }

    inline void Apply() {
        // update memory, save old memory state
        std::deque<std::unique_ptr<SharedMemoryCore>> oldMemory;
        for (const auto& [axiom, updates] : stabilityUpdates) {
            if (updates.empty()) continue;
            oldMemory.push_back(plankton::Copy(*axiom));
            for (const auto& update : updates) update();
        }
        
        // save old memory state as history
        for (auto& memory : oldMemory) {
            annotation->Conjoin(std::make_unique<PastPredicate>(std::move(memory)));
        }
        
        // DEBUG(" -- post: " << *annotation << std::endl;)
    }

    inline void Postprocess() {
        SymbolFactory emptyFactory;
        plankton::RenameSymbols(*annotation, emptyFactory);
    }

    inline std::unique_ptr<Annotation> GetResult() {
        return std::move(annotation);
    }
};

// bool Solver::IsMemoryImmutable(const SharedMemoryCore& memory, const Formula& context) const {
//     if (interference.empty()) return true;
//     MEASURE("Solver::IsMemoryImmutable")
//     auto annotation = std::make_unique<Annotation>();
//     annotation->Conjoin(plankton::Copy(memory));
//     annotation->Conjoin(plankton::Copy(context));
//     plankton::Simplify(*annotation);
//     InterferenceInfo info(plankton::Copy(*annotation), interference);
//     auto stable = info.GetResult();
//     return Implies(*stable, *annotation);
// }

std::unique_ptr<Annotation> Solver::MakeInterferenceStable(std::unique_ptr<Annotation> annotation) const {
    // TODO: should this take a list of annotations?
    if (interference.empty()) return annotation;
    if (plankton::Collect<SharedMemoryCore>(*annotation->now).empty()) return annotation;

    MEASURE("Solver::MakeInterferenceStable")
    DEBUG("<<INTERFERENCE>>" << std::endl)
    assert(!IsUnsatisfiable(*annotation));
    plankton::ExtendStack(*annotation, config, ExtensionPolicy::FAST);
    InterferenceInfo info(std::move(annotation), interference);
    auto result = info.GetResult();
    plankton::InlineAndSimplify(*result);
    DEBUG(*result << std::endl << std::endl)
    assert(!IsUnsatisfiable(*result));
    return result;
}