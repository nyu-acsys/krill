#include "default_solver.hpp"

#include "heal/util.hpp"
#include "heal/collect.hpp"
#include "encoder.hpp"
#include "post_helper.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


inline bool UpdatesFlow(const HeapEffect& effect) {
    return &effect.pre->flow.get() != &effect.post->flow.get();
}

inline bool UpdatesField(const HeapEffect& effect, const std::string& field) {
    return &effect.pre->fieldToValue.at(field)->Decl() != &effect.post->fieldToValue.at(field)->Decl();
}


//
// Stability
//

struct InterferenceInfo {
    SymbolicFactory factory;
    z3::context context;
    z3::solver solver;
    Z3Encoder<> encoder;
    ImplicationCheckSet checks;
    std::deque<std::unique_ptr<heal::Annotation>> annotations;
    const std::deque<std::unique_ptr<HeapEffect>>& interferences;
    std::map<const Annotation*, std::map<const PointsToAxiom*, std::deque<std::function<void()>>>> stabilityUpdates;

    InterferenceInfo(std::deque<std::unique_ptr<heal::Annotation>> annotations_, const std::deque<std::unique_ptr<HeapEffect>>& interferences_)
            : solver(context), encoder(context, solver), checks(context), annotations(std::move(annotations_)), interferences(interferences_) {
        std::cout << "<<INTERFERENCE>>" << std::endl;
        Preprocess();
        Compute();
        Apply();
        Postprocess();
    }

    inline void DebugPrint(const std::string& note, bool printEffects=false) {
        if (printEffects) for (auto& effect : interferences) std::cout << "  -- effect: " << *effect->pre << " ~~> " << *effect->post << " under " << *effect->context << std::endl;
        for (auto& annotation : annotations) std::cout << " -- " << note << ": " << *annotation << std::endl;
    }

    inline void Preprocess() {
        // make symbols distinct
        for (const auto& effect : interferences) {
            factory.Avoid(*effect->pre);
            factory.Avoid(*effect->post);
            factory.Avoid(*effect->context);
        }
        for (auto& annotation : annotations) {
            heal::RenameSymbolicSymbols(*annotation, factory);
            factory.Avoid(*annotation);
        }

        DebugPrint("pre", true);
    }

    inline void Handle(const Annotation& annotation, PointsToAxiom& memory, const HeapEffect& effect) {
        // TODO: avoid encoding the same annotation/effect multiple times
        if (memory.node->Type() != effect.pre->node->Type()) return;
        if (&effect.pre->node->Decl() != &effect.post->node->Decl()) throw std::logic_error("Unsupported effect"); // TODO: better error handling

        auto interference = encoder(annotation) && encoder(*effect.context) && encoder.MakeMemoryEquality(memory, *effect.pre);
        auto isInterferenceFree = z3::implies(interference, encoder.context.bool_val(false));
        auto applyEffect = [this,&memory,&effect](){
            if (UpdatesFlow(effect)) memory.flow = factory.GetUnusedFlowVariable(memory.flow.get().type);
            for (auto& [field, value] : memory.fieldToValue) {
                if (UpdatesField(effect, field)) value->decl_storage = factory.GetUnusedSymbolicVariable(value->Type());
            }
        };
        checks.Add(isInterferenceFree, [&,applyEffect=std::move(applyEffect)](bool isStable){
            if (isStable) return;
            stabilityUpdates[&annotation][&memory].push_back(std::move(applyEffect));
        });
    }

    inline void Compute() {
        for (auto& annotation : annotations) {
            auto resources = heal::CollectMemory(*annotation->now);
            for (auto* memory : resources) {
                if (memory->isLocal) continue;
                auto nonConstMemory = const_cast<PointsToAxiom*>(memory); // TODO: does this work?
                for (const auto& effect : interferences) {
                    Handle(*annotation, *nonConstMemory, *effect);
                }
            }
        }
        solver::ComputeImpliedCallback(solver, checks);
    }

    inline void Apply() {
        for (auto& annotation : annotations) {
            auto oldMemory = std::make_unique<SeparatingConjunction>();
            for (const auto& [axiom, updates] : stabilityUpdates[annotation.get()]) {
                if (updates.empty()) continue;
                oldMemory->conjuncts.push_back(heal::Copy(*axiom));
                for (const auto& update : updates) update();
            }
            annotation->time.push_back(std::make_unique<PastPredicate>(std::move(oldMemory)));
        }

        DebugPrint("post");
    }

    inline void Postprocess() {
        for (auto& annotation : annotations) {
            SymbolicFactory emptyFactory;
            heal::RenameSymbolicSymbols(*annotation, emptyFactory);
        }
    }

    inline std::deque<std::unique_ptr<heal::Annotation>> GetResult() {
        return std::move(annotations);
    }
};

std::deque<std::unique_ptr<heal::Annotation>> DefaultSolver::MakeStable(std::deque<std::unique_ptr<heal::Annotation>> annotations,
                                                                        const std::deque<std::unique_ptr<HeapEffect>>& interferences) const {
    if (annotations.empty() || interferences.empty()) return annotations;
    InterferenceInfo info(std::move(annotations), interferences);
    auto result = info.GetResult();
    for (auto& annotation : result) {
        annotation = Interpolate(std::move(annotation), interferences);
    }
    return result;
}


//
// Interpolation
//

struct ImmutabilityInfo {
    std::set<std::pair<const Type*, std::string>> mutableFields;

    explicit ImmutabilityInfo(const std::deque<std::unique_ptr<HeapEffect>>& interferences) {
        // TODO: compute immutability semantically => check if a field is immutable in a given state rather than across the entire computation
        for (const auto& effect : interferences) {
            auto& type = effect->pre->node->Type();
            for (const auto& [field, value] : effect->pre->fieldToValue) {
                if (UpdatesField(*effect, field)) mutableFields.emplace(&type, field);
            }
        }
    }

    [[nodiscard]] bool IsImmutable(const cola::Type& nodeType, const std::string& field) const {
        return mutableFields.count({ &nodeType, field }) == 0;
    }
};

std::unique_ptr<Annotation> DefaultSolver::Interpolate(std::unique_ptr<Annotation> annotation,
                                                       const std::deque<std::unique_ptr<HeapEffect>>& interferences) const {
    std::cout << "<<INTERPOLATION>>" << std::endl;
    std::cout << " ** pre: " << *annotation << std::endl;
    ImmutabilityInfo info(interferences); // TODO: avoid repeated construction => take deque of annotations

    // collect all points-to predicates from all times
    auto memories = heal::CollectMemory(*annotation->now);
    for (const auto& time : annotation->time) {
        if (auto past = dynamic_cast<const PastPredicate*>(time.get())) {
            auto more = heal::CollectMemory(*past->formula);
            memories.insert(more.begin(), more.end());
        }
    }

    // equalize immutable fields (ignore flow => surely not immutable)
    auto equalize = [&info,&annotation](const PointsToAxiom& memory, const PointsToAxiom& other){
        if (memory.node->Type() != other.node->Type()) return;
        if (&memory.node->Decl() != &other.node->Decl()) return;
        for (const auto& [field, value] : memory.fieldToValue) {
            if (!info.IsImmutable(memory.node->Type(), field)) continue;
            auto interpolant = std::make_unique<heal::SymbolicAxiom>(heal::Copy(*value), heal::SymbolicAxiom::EQ,
                                                                     heal::Copy(*other.fieldToValue.at(field)));
            annotation->now->conjuncts.push_back(std::move(interpolant));
        }
    };
    for (auto it1 = memories.cbegin(); it1 != memories.cend(); ++it1) {
        for (auto it2 = std::next(it1); it2 != memories.cend(); ++it2) {
            equalize(**it1, **it2);
        }
    }

//    annotation = solver::TryAddPureFulfillmentForHistories(std::move(annotation), Config());
    heal::Simplify(*annotation);
//    heal::InlineAndSimplify(*annotation);
    std::cout << " ** post: " << *annotation << std::endl;
    return annotation;
}
