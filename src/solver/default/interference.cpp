#include "default_solver.hpp"

#include "timer.hpp"
#include "heal/util.hpp"
#include "heal/collect.hpp"
#include "encoder.hpp"
#include "post_helper.hpp"
#include "expand.hpp"

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

std::unique_ptr<SeparatingConjunction> ComputeInterpolant(const Annotation& annotation, const std::deque<std::unique_ptr<HeapEffect>>& interferences) {
    std::cout << "<<INTERPOLATION>>" << std::endl;
    std::cout << " ** for: " << annotation << std::endl;
    ImmutabilityInfo info(interferences); // TODO: avoid repeated construction => take deque of annotations

    // collect all points-to predicates from all times
    auto memories = heal::CollectMemory(*annotation.now);
    for (const auto& time : annotation.time) {
        if (auto past = dynamic_cast<const PastPredicate*>(time.get())) {
            auto more = heal::CollectMemory(*past->formula);
            memories.insert(more.begin(), more.end());
        }
    }

    // equalize immutable fields (ignore flow => surely not immutable)
    auto result = std::make_unique<SeparatingConjunction>();
    auto equalize = [&info,&result](const PointsToAxiom& memory, const PointsToAxiom& other){
        if (memory.node->Type() != other.node->Type()) return;
        if (&memory.node->Decl() != &other.node->Decl()) return;
        for (const auto& [field, value] : memory.fieldToValue) {
            if (!info.IsImmutable(memory.node->Type(), field)) continue;
            auto interpolant = std::make_unique<heal::SymbolicAxiom>(heal::Copy(*value), heal::SymbolicAxiom::EQ,
                                                                     heal::Copy(*other.fieldToValue.at(field)));
            result->conjuncts.push_back(std::move(interpolant));
        }
    };
    for (auto it1 = memories.cbegin(); it1 != memories.cend(); ++it1) {
        for (auto it2 = std::next(it1); it2 != memories.cend(); ++it2) {
            equalize(**it1, **it2);
        }
    }

    std::cout << " ** result: " << *result << std::endl;
    return result;
}

std::unique_ptr<Annotation> PerformInterpolation(std::unique_ptr<Annotation> annotation, const std::deque<std::unique_ptr<HeapEffect>>& interferences) {
    annotation->now->conjuncts.push_back(ComputeInterpolant(*annotation, interferences));
    heal::Simplify(*annotation);
    return annotation;
}


inline std::unique_ptr<heal::SeparatingConjunction> RemoveResourcesAndFulfillments(std::unique_ptr<heal::SeparatingConjunction> formula) {
    heal::InlineAndSimplify(*formula);
    auto fulfillments = heal::CollectFulfillments(*formula);
    auto variables = heal::CollectVariables(*formula);
    auto memory = heal::CollectMemory(*formula);
    for (auto it = memory.begin(); it != memory.end();) {
        if ((**it).isLocal) it = memory.erase(it); // do not delete local resources (below)
        else ++it;
    }

    std::set<const heal::Formula*> axioms;
    axioms.insert(variables.begin(), variables.end());
    axioms.insert(memory.begin(), memory.end());
    axioms.insert(fulfillments.begin(), fulfillments.end());

    formula->conjuncts.erase(std::remove_if(formula->conjuncts.begin(), formula->conjuncts.end(), [&axioms](const auto& elem){
        auto find = axioms.find(elem.get());
        if (find == axioms.end()) return false;
        axioms.erase(find);
        return true;
    }), formula->conjuncts.end());

    assert(axioms.empty());
    return formula;
}

inline std::deque<PastPredicate*> CollectPast(Annotation& annotation) {
    std::deque<PastPredicate*> result;
    for (auto& time : annotation.time) {
        auto past = dynamic_cast<heal::PastPredicate*>(time.get());
        if (!past) continue;
        result.push_back(past);
    }
    return result;
}

inline std::unique_ptr<heal::Annotation>
TryAddPureFulfillmentForHistories(std::unique_ptr<heal::Annotation> annotation, const SolverConfig& config,
                                  const std::deque<std::unique_ptr<HeapEffect>>& interferences) {
    std::cout << "********* TryAddPureFulfillmentForHistories *********" << *annotation << std::endl;
    auto base = std::make_unique<Annotation>(RemoveResourcesAndFulfillments(heal::Copy(*annotation->now)));

    // prepare
    std::deque<std::unique_ptr<SeparatingConjunction>> states;
    for (const auto& time : annotation->time) {
        auto past = dynamic_cast<heal::PastPredicate*>(time.get());
        if (!past) continue;

        auto tmp = heal::Copy(*base);
        // base contains only local memory, so we can simply add the shared resources from past
        tmp->now->conjuncts.push_back(heal::Copy(*past->formula));
        tmp->now = solver::ExpandMemoryFrontier(std::move(tmp->now), config, past->formula.get());

        states.push_back(std::move(tmp->now));
    }

    // interpolate
    for (auto& state : states) {
        auto tmp = std::make_unique<Annotation>(heal::Copy(*annotation->now));
        tmp->time.push_back(std::make_unique<PastPredicate>(heal::Copy(*state)));
        state->conjuncts.push_back(ComputeInterpolant(*tmp, interferences));
        heal::Simplify(*state);
    }

    // try add fulfillment
    for (auto& state : states) {
        auto tmp = std::make_unique<Annotation>(std::move(state));
//        heal::InlineAndSimplify(*tmp);
        std::cout << "TryAddPureFulfillmentForHistories invokes TryAddPureFulfillment with " << *tmp << std::endl;
        tmp = TryAddPureFulfillment(std::move(tmp), config);

        for (const auto* fulfillment : heal::CollectFulfillments(*tmp)) {
            annotation->now->conjuncts.push_back(heal::Copy(*fulfillment)); // TODO: avoid copy
        }
    }

    std::cout << "TryAddPureFulfillmentForHistories post: " << *annotation << std::endl;
    return annotation;
}

std::unique_ptr<Annotation> DefaultSolver::Interpolate(std::unique_ptr<Annotation> annotation,
                                                       const std::deque<std::unique_ptr<HeapEffect>>& interferences) const {
    static Timer timer("DefaultSolver::Interpolate");
    auto measurement = timer.Measure();

    annotation = PerformInterpolation(std::move(annotation), interferences);
    annotation = TryAddPureFulfillment(std::move(annotation), Config());
    annotation = TryAddPureFulfillmentForHistories(std::move(annotation), Config(), interferences);
    return annotation;
}


//
// History stuff (hack)
//

std::set<const SymbolicVariableDeclaration*> IntersectExpansion(const std::set<const SymbolicVariableDeclaration*>& base, const PastPredicate& past) {
    struct Collector : public heal::DefaultLogicListener {
        std::set<const heal::SymbolicVariableDeclaration*> result;
        void enter(const heal::SymbolicVariable& obj) override {
            if (obj.Sort() != cola::Sort::PTR) return;
            result.insert(&obj.Decl());
        }
    } collector;
    past.formula->accept(collector);
    for (auto it = collector.result.begin(); it != collector.result.end();) {
        if (base.count(*it) == 0) it = collector.result.erase(it);
        else ++it;
    }
    return std::move(collector.result);
}

std::set<const PointsToAxiom*> CollectHistoricMemory(const Annotation& annotation) {
    std::set<const PointsToAxiom*> result;
    for (const auto& time : annotation.time) {
        if (auto past = dynamic_cast<const PastPredicate*>(time.get())) {
            if (auto mem = dynamic_cast<const PointsToAxiom*>(past->formula.get())) {
                result.insert(mem);
            }
        }
    }
    return result;
}

std::unique_ptr<Annotation> AddHistoryExpansions(std::unique_ptr<Annotation> annotation, const SolverConfig& config) {
    auto base = std::make_unique<Annotation>(RemoveResourcesAndFulfillments(heal::Copy(*annotation->now)));
    std::deque<std::unique_ptr<PastPredicate>> newHistory;
    auto newStack = std::make_unique<SeparatingConjunction>();

    // expand memory only for addresses that are referenced by a variable (everything else is lost by the join anyways)
    std::set<const SymbolicVariableDeclaration*> expansion;
    for (const auto* resource : heal::CollectVariables(*annotation->now)) {
        expansion.insert(&resource->value->Decl());
    }
    heal::SymbolicFactory factory(*annotation);

    // go over past memory and improve knowledge
    for (const auto& time : annotation->time) {
        auto past = dynamic_cast<heal::PastPredicate*>(time.get());
        if (!past) continue;

        auto tmp = heal::Copy(*base);
        // base contains only local memory, so we can simply add the shared resources from past
        tmp->now->conjuncts.push_back(heal::Copy(*past->formula));

        // find new memories
        auto existingMemories = heal::CollectMemory(*tmp->now);
        tmp->now = solver::ExpandMemoryFrontier(std::move(tmp->now), factory, config, IntersectExpansion(expansion, *past));
//        tmp->now = solver::ExpandMemoryFrontier(std::move(tmp->now), Config(), past->formula.get());
        auto newMemories = heal::CollectMemory(*tmp->now);
        bool foundNewMemory = false;
        for (const auto* memory : newMemories) {
            if (existingMemories.count(memory) != 0) continue;
            newHistory.push_back(std::make_unique<PastPredicate>(heal::Copy(*memory)));
            foundNewMemory = true;
        }
        if (!foundNewMemory) continue; // we expect to already know everything about the existing memories

        // extend stack
        auto graph = solver::MakePureHeapGraph(std::move(tmp), config);
        assert(!graph.nodes.empty());
        EncodedFlowGraph encoding(std::move(graph));
        FootprintChecks checks(encoding.context);
        auto candidates = CandidateGenerator::Generate(encoding.graph, EMode::PRE, CandidateGenerator::FlowLevel::FAST);
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            checks.Add(encoding.encoder(*candidates[index]), [&candidates,index,target=newStack.get()](bool holds){
//                std::cout << "  |implies=" << holds << "|  " << *checks.candidates[index] << std::endl;
                if (!holds) return;
                target->conjuncts.push_back(std::move(candidates[index]));
            });
        }
        solver::ComputeImpliedCallback(encoding.solver, checks.checks);
    }

    annotation->now->conjuncts.push_back(std::move(newStack));
    std::move(newHistory.begin(), newHistory.end(), std::back_inserter(annotation->time));
    heal::Simplify(*annotation);

    return annotation;
}

template<typename T>
bool EmptyIntersection(const PastPredicate& past, const T& container) { // TODO: can be implemented more efficiently
    if (auto mem = dynamic_cast<const PointsToAxiom*>(past.formula.get())) {
        auto contains = [&container](const VariableDeclaration* decl){ return container.count(decl) != 0; };
        if (contains(&mem->flow.get())) return false;
        for (const auto& [field, value] : mem->fieldToValue) {
            if (contains(&value->Decl())) return false;
        }
        return true;
    }
    return false;
}

std::unique_ptr<Annotation> AddSemanticHistoryInterpolation(std::unique_ptr<Annotation> annotation,
                                                            const std::deque<std::unique_ptr<HeapEffect>>& interferences,
                                                            const SolverConfig& config) {
    static Timer timer("AddSemanticHistoryInterpolation");
    auto measurement = timer.Measure();

    auto pastMemories = CollectHistoricMemory(*annotation);
    if (pastMemories.empty()) return annotation;
    auto nowMemories = heal::CollectMemory(*annotation->now);

    // prepare solving
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    SymbolicFactory factory;
    for (const auto& effect : interferences) {
        factory.Avoid(*effect->pre);
        factory.Avoid(*effect->post);
        factory.Avoid(*effect->context);
    }
    heal::RenameSymbolicSymbols(*annotation, factory);
    factory.Avoid(*annotation);
    solver.add(encoder(*annotation));

    std::cout << "=== Semantic Interpolation for: " << *annotation << std::endl;
    for (auto& effect : interferences) std::cout << "  -- effect: " << *effect->pre << " ~~> " << *effect->post << " under " << *effect->context << std::endl;

    // feed solver with knowledge about interpolants
    std::deque<std::pair<PastPredicate, z3::expr>> interpolation;
    for (const auto* now : nowMemories) {
        for (const auto* past : pastMemories) {
            if (&now->node->Decl() != &past->node->Decl()) continue;
            if (now->isLocal || past->isLocal) continue;

            for (const auto& [field, nowValue] : now->fieldToValue) {
                auto& pastValue = past->fieldToValue.at(field);
                if (&nowValue->Decl() == &pastValue->Decl()) continue;

//                std::cout << " ||| field=" << field << " for " << *now << "  vs  " << *past << std::endl;
                auto newHistory = MakePointsTo(now->node->Decl(), factory, config);
                auto& histValue = newHistory->fieldToValue.at(field);

                z3::expr_vector vector(encoder.context);
                vector.push_back( // field was never changed
                        encoder(*nowValue) == encoder(*pastValue)
                        // && encoder(*pastValue) == encoder(*histValue)
                        && encoder.MakeMemoryEquality(*past, *newHistory)
                );
//                std::cout << "       ~> " << *nowValue << " == " << *pastValue << " AND " << *past << " == " << *newHistory << std::endl;
                for (const auto& effect : interferences) {
                    if (!UpdatesField(*effect, field)) continue;
                    auto& effectValue = effect->post->fieldToValue.at(field);
                    vector.push_back(
                            encoder(*nowValue) != encoder(*pastValue)
                            && encoder(*nowValue) == encoder(*histValue)
                            // && encoder(*effectValue) == encoder(*histValue)
                            && encoder(*effect->context)
                            && encoder.MakeMemoryEquality(*effect->post, *newHistory)
                    );
//                    std::cout << "       ~> " << *nowValue << " != " << *pastValue << " AND " << *nowValue << " == " << *histValue << " AND " << *effect->context << " AND " << *effect->post << " == " << *newHistory << std::endl;
                }

//                solver.add(z3::mk_or(vector));
                auto premise = z3::mk_or(vector);
                interpolation.emplace_back(std::move(newHistory), premise);
//                std::cout << " ||| field=" << field << " for " << *now << "  vs  " << *past << std::endl << premise << std::endl;
            }
        }
    }

    // derive knowledge
    if (interpolation.empty()) return annotation;
    for (const auto& [past, expr] : interpolation) annotation->time.push_back(heal::Copy(past));
    auto candidates = CandidateGenerator::Generate(*annotation, CandidateGenerator::FAST);
    ImplicationCheckSet checks(context);
    std::set<std::size_t> implied;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        auto& candidate = candidates[index];
        auto encoded = encoder(*candidate);
        auto candidateSymbols = heal::CollectSymbolicSymbols(*candidate);
        for (const auto& [past, premise] : interpolation) {
            auto pastSymbols = heal::CollectSymbolicSymbols((past)); // TODO: avoid repeated collection
            if (EmptyIntersection(past, candidateSymbols)) continue;
            checks.Add(z3::implies(premise, encoded), [&implied,index](bool holds){
                if (holds) implied.insert(index);
            });
        }
    }
    std::cout << " ~> going to solve; #interpolation=" << interpolation.size() << " #checks=" << checks.expressions.size() << " #candidates=" << candidates.size() << std::endl;
    bool isSolverUnsat;
    solver::ComputeImpliedCallback(solver, checks, &isSolverUnsat);
    if (isSolverUnsat) throw std::logic_error("Unexpected unsatisfiability during semantic interpolation."); // TODO: remove

    for (auto index : implied) {
        assert(candidates[index]);
        annotation->now->conjuncts.push_back(std::move(candidates[index]));
    }

    std::cout << "=== Semantic Interpolation yields: " << *annotation << std::endl;
//    throw std::logic_error("interpolation inspection point");
    return annotation;
}

template<typename T>
bool NonEmptyIntersection(const T& container, const T& other) {
    // TODO: can be done more efficiently for sets
    auto find = std::find_if(container.begin(), container.end(), [&other](const auto* elem){ return other.count(elem) != 0; });
    return find != container.end();
}

z3::expr MakeConsumptionCheck(Z3Encoder<>& encoder, const Annotation& annotation, const PointsToAxiom& stronger, const PointsToAxiom& weaker) {
    auto strongerSymbols = CollectSymbolicSymbols(stronger);
    auto weakerSymbols = CollectSymbolicSymbols(weaker);

    z3::expr_vector strongerEncoding(encoder.context);
    z3::expr_vector weakerEncoding(encoder.context);
    for (const auto& conjunct : annotation.now->conjuncts) {
        auto symbols = CollectSymbolicSymbols(*conjunct);
        bool containsWeak = NonEmptyIntersection(symbols, weakerSymbols);
        bool containsStrong = NonEmptyIntersection(symbols, strongerSymbols);
        if (containsStrong) strongerEncoding.push_back(encoder(*conjunct));
        if (containsWeak) weakerEncoding.push_back(encoder(*conjunct));
    }
//    auto strongerEncoding = std::make_unique<SeparatingConjunction>();
//    auto weakerEncoding = std::make_unique<SeparatingConjunction>();
//    for (const auto& conjunct : annotation.now->conjuncts) {
//        auto symbols = CollectSymbolicSymbols(*conjunct);
//        bool containsWeak = NonEmptyIntersection(symbols, weakerSymbols);
//        bool containsStrong = NonEmptyIntersection(symbols, strongerSymbols);
//        if (containsStrong) strongerEncoding->conjuncts.push_back(heal::Copy(*conjunct));
//        if (containsWeak) weakerEncoding->conjuncts.push_back(heal::Copy(*conjunct));
//    }

    auto eq = encoder.MakeMemoryEquality(stronger, weaker);
    return z3::implies(z3::mk_and(strongerEncoding) && eq, z3::mk_and(weakerEncoding));
//    return z3::implies(encoder(*strongerEncoding) && eq, encoder(*weakerEncoding));
}

std::unique_ptr<Annotation> FilterOutHistories(std::unique_ptr<Annotation> annotation, const SolverConfig& config) {
//    annotation = MakeHistorySymbolsUnique(std::move(annotation));
    annotation = ExtendStack(std::move(annotation), config); // makes filter more precise

    heal::InlineAndSimplify(*annotation);
    auto pastMemories = CollectHistoricMemory(*annotation);
    if (pastMemories.empty()) return annotation;
//    std::cout << "FilterOutHistories for " << *annotation << std::endl;

    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    ImplicationCheckSet checks(context);

    std::set<const Formula*> unnecessary;
    std::set<std::pair<const PointsToAxiom*, const PointsToAxiom*>> blacklist;
    for (auto it = pastMemories.begin(); it != pastMemories.end(); ++it) {
        for (auto other = std::next(it); other != pastMemories.end(); ++other) {
            if (&(*it)->node->Decl() != &(*other)->node->Decl()) continue;
            auto eq = encoder.MakeMemoryEquality(**it, **other);
            checks.Add(!eq, [&blacklist,mem1=*it,mem2=*other](bool holds){
                if (!holds) return;
                blacklist.emplace(mem1, mem2);
                blacklist.emplace(mem2, mem1);
            });
        }
    }
    for (auto elem : pastMemories) {
        for (auto other : pastMemories) {
            if (elem == other) continue;
            if (&elem->node->Decl() != &other->node->Decl()) continue;
            auto consumes = MakeConsumptionCheck(encoder, *annotation, *elem, *other);
            checks.Add(consumes, [&unnecessary,&blacklist,elem,other](bool holds) {
                if (!holds) return;
                if (blacklist.count({ elem, other }) != 0) return;
                blacklist.emplace(other, elem); // break circular consumptions
                unnecessary.insert(other);
            });
        }
    }
    solver::ComputeImpliedCallback(solver, checks);
//    for (auto* elem : unnecessary) std::cout << "  => removing: " << *elem << "  " << std::endl;

    annotation->time.erase(std::remove_if(annotation->time.begin(), annotation->time.end(), [&unnecessary](const auto& elem){
        if (auto past = dynamic_cast<const PastPredicate*>(elem.get())) {
            return unnecessary.count(past->formula.get()) != 0;
        }
        return false;
    }), annotation->time.end());

    return annotation;
}

std::unique_ptr<Annotation> DefaultSolver::TryFindBetterHistories(std::unique_ptr<Annotation> annotation,
                                                                  const std::deque<std::unique_ptr<HeapEffect>>& interferences) const {
    if (interferences.empty()) return annotation;
    static Timer timer("DefaultSolver::TryFindBetterHistories");
    auto measurement = timer.Measure();

    std::cout << "__ TryFindBetterHistories for " << *annotation << std::endl;

    // TODO: ---- this is a hack to get better histories from joins ---
    // TODO: this is tailored towards what the join needs
    // TODO: this code is copied/adapted from above
    // TODO: this code is copied from ExtendStack
    // TODO: this code is copied from ExpandMemoryFrontier
    // TODO: does this work as intended? check soundness carefully again!
    // TODO: avoid two filter steps

    heal::InlineAndSimplify(*annotation);
    annotation = AddHistoryExpansions(std::move(annotation), Config());
    annotation->now->conjuncts.push_back(ComputeInterpolant(*annotation, interferences));
    annotation = FilterOutHistories(std::move(annotation), Config());
    annotation = AddSemanticHistoryInterpolation(std::move(annotation), interferences, Config());
    annotation->now->conjuncts.push_back(ComputeInterpolant(*annotation, interferences));
    annotation = FilterOutHistories(std::move(annotation), Config());
    heal::InlineAndSimplify(*annotation);

    std::cout << "__ TryFindBetterHistories yields " << *annotation << std::endl;

    return annotation;
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

//    static inline std::deque<std::unique_ptr<PastPredicate>> MakeHistory(const Annotation& annotation, std::deque<std::unique_ptr<PointsToAxiom>>&& oldMemory) {
//        // collect addresses that already have history information
//        std::set<const SymbolicVariableDeclaration*> covered;
//        for (const auto& time : annotation.time) {
//            if (auto past = dynamic_cast<const PastPredicate*>(time.get())) {
//                auto nodes = heal::CollectMemoryNodes(*past->formula);
//                covered.insert(nodes.begin(), nodes.end());
//            }
//        }
//
//        // add history information for nodes that have no history information already
//        std::deque<std::unique_ptr<PastPredicate>> result;
//        for (auto&& memory : oldMemory) {
//
//        }
//        return result;
//    }

//    static inline std::set<const SymbolicVariableDeclaration*> GetHistoricNodes(const Annotation& annotation) {
//        std::set<const SymbolicVariableDeclaration*> result;
//        for (const auto& time : annotation.time) {
//            if (auto past = dynamic_cast<const PastPredicate*>(time.get())) {
//                auto nodes = heal::CollectMemoryNodes(*past->formula);
//                result.insert(nodes.begin(), nodes.end());
//            }
//        }
//        return result;
//    }

    inline void Apply() {
        for (auto& annotation : annotations) {
//            auto historicMemory = GetHistoricNodes(*annotation);
//            auto addToPast = [&historicMemory,&annotation](const PointsToAxiom& memory){
//                if (memory.isLocal) return;
//                if (historicMemory.count(&memory.node->Decl()) != 0) return; // TODO: is this a good idea?
//                annotation->time.push_back(std::make_unique<PastPredicate>(heal::Copy(memory)));
//            };
            auto addToPast = [&annotation](const PointsToAxiom& memory){
                if (memory.isLocal) return;
                annotation->time.push_back(std::make_unique<PastPredicate>(heal::Copy(memory)));
            };

            // update memory, move old memory to history if not yet covered
            std::deque<std::unique_ptr<PointsToAxiom>> oldMemory;
            for (const auto& [axiom, updates] : stabilityUpdates[annotation.get()]) {
                if (updates.empty()) continue;
                addToPast(*axiom);
                for (const auto& update : updates) update();
            }
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
    static Timer timer("DefaultSolver::MakeStable");
    auto measurement = timer.Measure();

    if (annotations.empty() || interferences.empty()) return annotations;
    for (auto& annotation : annotations) annotation = TryFindBetterHistories(std::move(annotation), interferences);
    InterferenceInfo info(std::move(annotations), interferences);
    auto result = info.GetResult();
//    for (auto& annotation : result) {
//        annotation = PerformInterpolation(std::move(annotation), interferences);
//    }
    return result;
}
