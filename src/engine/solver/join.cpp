#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;

constexpr const bool DEEP_PREPROCESSING = false;
constexpr const ExtensionPolicy EXTENSION = ExtensionPolicy::FAST;


inline std::unique_ptr<StackAxiom> MakeEq(const SymbolDeclaration& decl, const SymbolDeclaration& other) {
    return std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(decl),
                                        std::make_unique<SymbolicVariable>(other));
}

inline std::unique_ptr<EqualsToAxiom> MakeEqualsTo(const VariableDeclaration& decl, SymbolFactory& factory) {
    return std::make_unique<EqualsToAxiom>(decl, factory.GetFreshFO(decl.type));
}

inline std::unique_ptr<ObligationAxiom> MakeObligation(const SymbolDeclaration& key, Specification spec) {
    return std::make_unique<ObligationAxiom>(spec, key);
}

inline std::unique_ptr<FulfillmentAxiom> MakeFulfillment(bool value) {
    return std::make_unique<FulfillmentAxiom>(value);
}

struct AnnotationInfo {
    const Annotation& annotation;
    std::map<const VariableDeclaration*, const EqualsToAxiom*> varToRes;
    std::map<const VariableDeclaration*, const MemoryAxiom*> varToMem;
    std::size_t numTrueFulfillments = 0, numFalseFulfillments = 0;
    std::map<const VariableDeclaration*, std::set<const ObligationAxiom*>> varToObl;
    std::map<const VariableDeclaration*, std::set<const PastPredicate*>> varToHist;
    std::map<const VariableDeclaration*, std::set<const FuturePredicate*>> varToFut;

    explicit AnnotationInfo(const Annotation& annotation) : annotation(annotation) {
        // variable + memory resource
        auto variables = plankton::Collect<EqualsToAxiom>(*annotation.now);
        auto obligations = plankton::Collect<ObligationAxiom>(*annotation.now);
        for (const auto* variable : variables) {
            // variable resources
            varToRes[&variable->Variable()] = variable;

            // memory resources
            if (variable->value->Sort() == Sort::PTR) {
                auto memory = plankton::TryGetResource(variable->Value(), *annotation.now);
                if (!memory) continue;
                varToMem[&variable->Variable()] = memory;
            }
            
            // obligation resources
            if (variable->value->Sort() == Sort::DATA) {
                for (const auto* obligation : obligations) {
                    if (obligation->key->Decl() != variable->Value()) continue;
                    varToObl[&variable->Variable()].insert(obligation);
                }
            }
        }
        
        // fulfillment resources
        auto fulfillments = plankton::Collect<FulfillmentAxiom>(*annotation.now);
        for (const auto* fulfillment : fulfillments) {
            if (fulfillment->returnValue) ++numTrueFulfillments;
            else ++numFalseFulfillments;
        }
        
        // past
        for (const auto& past : annotation.past) {
            for (const auto& [variable, resource] : varToRes) {
                if (resource->Value() != past->formula->node->Decl()) continue;
                varToHist[variable].insert(past.get());
            }
        }

        // future
        for (const auto& future : annotation.future) {
            assert(future->pre->node->Decl() == future->post->node->Decl());
            for (const auto& [variable, resource] : varToRes) {
                if (resource->Value() != future->pre->node->Decl()) continue;
                varToFut[variable].emplace(future.get());
            }
        }
    }
};

template<typename T> using SetIterator = typename std::set<const T*>::iterator;
template<typename T> using SetIteratorPair = std::pair<SetIterator<T>, SetIterator<T>>;

template<typename T>
inline std::deque<std::vector<const T*>>
MakePowerSet(std::vector<AnnotationInfo>& lookup, const VariableDeclaration& variable,
             std::function<SetIteratorPair<T>(AnnotationInfo&, const VariableDeclaration&)> getIterators) {
    std::vector<SetIterator<T>> begin, end;
    begin.reserve(lookup.size());
    end.reserve(lookup.size());
    bool containsEmpty = false;
    for (auto& info : lookup) {
        auto [infoBegin, infoEnd] = getIterators(info, variable);
        begin.push_back(infoBegin);
        end.push_back(infoEnd);
        containsEmpty |= infoBegin == infoEnd;
    }
    if (containsEmpty) return {};

    // iterator progression
    auto cur = begin;
    auto next = [&cur,&begin,&end]() -> bool {
        bool isEnd = true;
        for (std::size_t index = 0; index < cur.size(); ++index) {
            auto& it = cur[index];
            it.operator++();
            if (it == end[index]) it = begin[index];
            else isEnd = false;
        }
        return !isEnd;
    };

    // create power set
    std::deque<std::vector<const T*>> result;
    do {
        result.emplace_back();
        result.back().reserve(cur.size());
        for (auto it : cur) result.back().push_back(*it);
    } while (next());
    return result;
}

inline auto MakePastPowerSet(std::vector<AnnotationInfo>& lookup, const VariableDeclaration& variable) {
    return MakePowerSet<PastPredicate>(lookup, variable, [](auto& info, auto& variable) {
        auto& set = info.varToHist[&variable];
        return std::make_pair(set.begin(), set.end());
    });
}

inline auto MakeFuturePowerSet(std::vector<AnnotationInfo>& lookup, const VariableDeclaration& variable) {
    return MakePowerSet<FuturePredicate>(lookup, variable, [](auto& info, auto& variable) {
        auto& set = info.varToFut[&variable];
        return std::make_pair(set.begin(), set.end());
    });
}

inline void AddResourceCopies(Annotation& annotation, const SymbolDeclaration& src, const SymbolDeclaration& dst) {
    annotation.Conjoin(MakeEq(src, dst));

    // copy memory
    if (auto mem = plankton::TryGetResource(src, *annotation.now)) {
        auto newMem = plankton::Copy(*mem);
        newMem->node->decl = dst;
        annotation.Conjoin(std::move(newMem));
    }

    // copy pasts
    for (const auto& past : annotation.past) {
        if (past->formula->node->Decl() != src) continue;
        auto newPast = plankton::Copy(*past);
        newPast->formula->node->decl = dst;
        annotation.Conjoin(std::move(newPast));
    }

    // copy futures
    for (const auto& future : annotation.future) {
        assert(future->pre->node->Decl() == future->post->node->Decl());
        if (future->pre->node->Decl() != src) continue;
        auto newFuture = plankton::Copy(*future);
        newFuture->pre->node->decl = dst;
        newFuture->post->node->decl = dst;
        annotation.Conjoin(std::move(newFuture));
    }
}

struct AnnotationJoiner {
    std::unique_ptr<Annotation> result;
    std::deque<std::unique_ptr<Annotation>> annotations;
    SymbolFactory factory;
    const SolverConfig& config;
    std::vector<AnnotationInfo> lookup;
    std::map<const VariableDeclaration*, EqualsToAxiom*> varToCommonRes;
    std::map<const VariableDeclaration*, MemoryAxiom*> varToCommonMem;
    Encoding encoding;
    
    explicit AnnotationJoiner(std::deque<std::unique_ptr<Annotation>>&& annotations_, const SolverConfig& config)
            : result(std::make_unique<Annotation>()), annotations(std::move(annotations_)), config(config) {
    }

    std::unique_ptr<Annotation> GetResult() {
        assert(!annotations.empty());
        if (annotations.size() == 1) return std::move(annotations.front());

        // issue warning if future predicates are lost
        // TODO: handle futures
        auto losesFutures = plankton::Any(annotations, [](const auto& elem){ return !elem->future.empty(); });
        if (losesFutures) WARNING("join loses future predicates.")
        
        Preprocess();
        
        DEBUG("preprocessed, now joining: ")
        for (const auto& elem : annotations) DEBUG(*elem; std::cout << std::endl)

        CreateVariableResources();
        CreateMemoryResources();
        CreateObligationResources();
        CreateFulfillmentResources();
        EncodeNow();
        EncodePast();
        EncodeFuture();
        DeriveJoinedStack();

        return std::move(result);
    }

    inline void Preprocess() {
        DEBUG("[join] starting preprocessing..." << std::flush)
        lookup.reserve(annotations.size());
        for (auto& annotation : annotations) {
            // plankton::InlineAndSimplify(*annotation);
    
            // more memory
            std::set<const SymbolDeclaration*> expand;
            for (const auto* var : plankton::Collect<EqualsToAxiom>(*annotation->now))
                expand.insert(&var->Value());
            plankton::MakeMemoryAccessible(*annotation, std::move(expand), config); // TODO: ensure that the memory is shared!

            // more flow
            if constexpr (DEEP_PREPROCESSING) {
                Encoding enc;
                enc.AddPremise(*annotation->now);
                enc.AddPremise(enc.EncodeInvariants(*annotation->now, config));
                enc.AddPremise(enc.EncodeSimpleFlowRules(*annotation->now, config));
                plankton::ExtendStack(*annotation, enc, EXTENSION);
            }

            // distinct symbols across annotations
            plankton::RenameSymbols(*annotation, factory);
            
            // distinct values across variables
            auto resources = plankton::CollectMutable<EqualsToAxiom>(*annotation->now);
            std::set<const SymbolDeclaration*> used;
            for (auto* variable : resources) {
                if (variable->Variable().type.sort != Sort::PTR) continue;
                auto insertion = used.insert(&variable->Value());
                if (insertion.second) continue; // not yet in use
                auto& newValue = factory.GetFreshFO(variable->value->Type());
                AddResourceCopies(*annotation, variable->Value(), newValue);
                variable->value->decl = newValue;
            }
            
            // make info
            lookup.emplace_back(*annotation);
        }
        DEBUG(" done" << std::endl)
    }

    inline void CreateVariableResources() {
        // ensure all annotations have the same variables
        for (auto it = lookup.begin(); it != lookup.end(); ++it) {
            for (auto ot = std::next(it); ot != lookup.end(); ++ot) {
                auto& itVars = it->varToRes;
                auto& otVars = ot->varToRes;
                auto sameVariables = itVars.size() == otVars.size() &&
                        std::equal(itVars.begin(), itVars.end(), otVars.begin(), otVars.end(),
                                   [](auto& lhs, auto& rhs) { return lhs.first == rhs.first; });
                if (sameVariables) continue;
                throw std::logic_error("Cannot join: number of variable resources mismatches."); // TODO: better error handling
            }
        }
        
        // create resources
        assert(!lookup.empty());
        for (const auto& pair : lookup.front().varToRes) {
            auto& variable = *pair.first;
            auto newResource = MakeEqualsTo(variable, factory);
            varToCommonRes[&variable] = newResource.get();
            result->Conjoin(std::move(newResource));
        }
    }

    inline void CreateMemoryResources() {
        enum MemVis { LOCAL, SHARED, NONE };
        std::set<const SymbolDeclaration*> blacklist;
        
        auto getMemories = [this](const VariableDeclaration& decl) {
            std::vector<const MemoryAxiom*> memories;
            memories.reserve(lookup.size());
            for (auto& info : lookup) memories.push_back(info.varToMem[&decl]);
            return memories;
        };
        
        auto getCommonMemoryVisibility = [&blacklist](const std::vector<const MemoryAxiom*>& memories) {
            bool hasShared = false, hasLocal = false;
            for (const auto* memory : memories) {
                if (!memory) return NONE;
                if (plankton::Membership(blacklist, &memory->node->Decl())) return NONE;
                if (plankton::IsLocal(*memory)) hasLocal = true;
                else hasShared = true;
                if (hasShared && hasLocal) return NONE;
            }
            if (hasShared) return SHARED;
            if (hasLocal) return LOCAL;
            return NONE;
        };
        
        auto addToBlacklist = [&blacklist](const std::vector<const MemoryAxiom*>& memories) {
            for (const auto* elem : memories) blacklist.insert(&elem->node->Decl());
        };
        
        auto makeMemory = [this](MemVis vis, const SymbolDeclaration& address) -> std::unique_ptr<MemoryAxiom> {
            assert(vis != NONE);
            auto& flowType = config.GetFlowValueType();
            switch (vis) {
                case LOCAL: return plankton::MakeLocalMemory(address, flowType, factory);
                case SHARED: return plankton::MakeSharedMemory(address, flowType, factory);
                default: throw;
            }
        };
        
        // find matching memories
        for (const auto& [var, resource] : varToCommonRes) {
            if (var->type.sort != Sort::PTR) continue;
            
            // check if common memory exists
            auto memories = getMemories(*var);
            auto common = getCommonMemoryVisibility(memories);
            if (common == NONE) continue;
            addToBlacklist(memories);
            
            // add common memory
            auto commonMem = makeMemory(common, resource->Value());
            varToCommonMem[var] = commonMem.get();
            result->Conjoin(std::move(commonMem));
        }
        
        // ensure no local memories have been dropped
        auto localCount = plankton::Collect<LocalMemoryResource>(*result->now).size();
        for (const auto& annotation : annotations) {
            auto otherCount = plankton::Collect<LocalMemoryResource>(*annotation->now).size();
            if (localCount == otherCount) continue;
            throw std::logic_error("Unsupported join: loses local resource."); // TODO: better error handling
        }
    }

    inline void CreateObligationResources() {
        std::set<const ObligationAxiom*> blacklist;
        
        auto getSpecs = [](const auto& obligations) {
            std::vector<Specification> specs;
            specs.reserve(obligations.size());
            for (const auto* elem : obligations) specs.push_back(elem->spec);
            return specs;
        };
        
        auto isCommon = [this,&blacklist](const VariableDeclaration& decl, Specification spec) {
            std::set<const ObligationAxiom*> choice;
            for (auto& info : lookup) {
                auto* resource = info.varToRes[&decl];
                if (!resource) return false;
                auto& searchSet = info.varToObl[&decl];
                auto find = FindIf(searchSet, [&blacklist,&resource,spec](const auto* obligation) {
                    return obligation->spec == spec && obligation->key->Decl() == resource->Value() &&
                           !plankton::Membership(blacklist, obligation);
                });
                if (find == searchSet.end()) return false;
                choice.insert(*find);
            }
            plankton::InsertInto(std::move(choice), blacklist);
            return true;
        };
        
        for (const auto& [variable, resource] : varToCommonRes) {
            for (auto spec : getSpecs(lookup.front().varToObl[variable])) {
                auto common = isCommon(*variable, spec);
                if (!common) continue;
                result->Conjoin(MakeObligation(resource->Value(), spec));
            }
        }
    }
    
    inline void CreateFulfillmentResources() {
        std::size_t numTrue = lookup.front().numTrueFulfillments;
        std::size_t numFalse = lookup.front().numFalseFulfillments;
        for (const auto& info : lookup) {
            numTrue = std::min(numTrue, info.numTrueFulfillments);
            numFalse = std::min(numFalse, info.numFalseFulfillments);
        }
        for (std::size_t index = 0; index < numTrue; ++index) result->Conjoin(MakeFulfillment(true));
        for (std::size_t index = 0; index < numFalse; ++index) result->Conjoin(MakeFulfillment(false));
    }

    inline void EncodeNow() {
        std::vector<EExpr> disjunction;
        disjunction.reserve(lookup.size());

        for (auto& info : lookup) {
            std::vector<EExpr> encoded;
            encoded.reserve(16);

            // encode annotation + invariants
            encoded.push_back(encoding.Encode(*info.annotation.now));
            if constexpr (!DEEP_PREPROCESSING) {
                encoded.push_back(encoding.EncodeInvariants(*info.annotation.now, config));
                encoded.push_back(encoding.EncodeSimpleFlowRules(*info.annotation.now, config));
            }

            // force common variables to agree
            for (const auto& [var, resource] : varToCommonRes) {
                auto other = info.varToRes.at(var);
                encoded.push_back(encoding.Encode(resource->Value()) == encoding.Encode(other->Value()));
            }

            // force common memory to agree
            for (const auto& [var, memory] : varToCommonMem) {
                auto other = info.varToMem.at(var);
                encoded.push_back(encoding.EncodeMemoryEquality(*memory, *other));
            }

            disjunction.push_back(encoding.MakeAnd(encoded));
        }
        encoding.AddPremise(encoding.MakeOr(disjunction));
    }

    inline void EncodePast() {
        for (const auto&[variable, resource] : varToCommonRes) {
            auto powerSet = MakePastPowerSet(lookup, *variable);
            for (const auto& vector : powerSet) {
                // DEBUG(" join past combination:  ")
                // for (const auto* elem : vector) DEBUG(*elem << ",  ")
                // DEBUG(std::endl)

                auto& address = resource->Value();
                auto pastMem = plankton::MakeSharedMemory(address, config.GetFlowValueType(), factory);

                // force common past memory to agree
                auto encoded = plankton::MakeVector<EExpr>(vector.size());
                for (std::size_t index = 0; index < vector.size(); ++index) {
                    auto& now = *lookup.at(index).annotation.now;
                    auto memEq = encoding.EncodeMemoryEquality(*pastMem, *vector.at(index)->formula);
                    auto context = encoding.Encode(now) && encoding.EncodeInvariants(now, config);
                    encoded.push_back(memEq && context);
                }
                encoding.AddPremise(encoding.MakeOr(encoded));

                // add past predicate
                result->Conjoin(std::make_unique<PastPredicate>(std::move(pastMem)));
            }
        }
    }

    inline void EncodeFuture() {
        for (const auto&[variable, resource] : varToCommonRes) {
            auto powerSet = MakeFuturePowerSet(lookup, *variable);
            for (const auto& vector : powerSet) {
                DEBUG(" join future combination:  " << std::endl)
                for (const auto* elem : vector) DEBUG("  - " << *elem << std::endl)

                auto& address = resource->Value();
                auto futPreMem = plankton::MakeSharedMemory(address, config.GetFlowValueType(), factory);
                auto futPostMem = plankton::MakeSharedMemory(address, config.GetFlowValueType(), factory);
                auto futContext = std::make_unique<Annotation>();

                throw;
                // TODO: for every future, rename the symbols from pre/post to the ones from futPreMem/futPostMem and take the resulting context

                // conjoin future pre to generate context (make sure that no premise is lost)
                Encoding contextEncoding;
                auto contexts = plankton::MakeVector<EExpr>(vector.size());
                for (const auto* future : vector) {
                    contextEncoding.AddPremise(contextEncoding.EncodeMemoryEquality(*futPreMem, *future->pre));
                    contexts.push_back(contextEncoding.Encode(*future->context));
                }
                auto ctx = contextEncoding.MakeAnd(contexts);
                contextEncoding.Push();
                contextEncoding.AddPremise(ctx);
                plankton::ExtendStack(*futContext, contextEncoding, ExtensionPolicy::FAST);
                DEBUG(" joined context: " << *futContext << std::endl);
                contextEncoding.Pop();
                contextEncoding.AddPremise(*futContext->now);
                if (!contextEncoding.Implies(ctx)) {
                    DEBUG("    => discarding combination" << std::endl)
                    continue;
                }

                // force common future post to agree
                auto post = plankton::MakeVector<EExpr>(vector.size());
                for (std::size_t index = 0; index < vector.size(); ++index) {
                    auto& now = *lookup.at(index).annotation.now;
                    auto memEqPost = encoding.EncodeMemoryEquality(*futPostMem, *vector.at(index)->post);
                    auto more = encoding.Encode(now) && encoding.EncodeInvariants(now, config);
                    post.push_back(memEqPost && more);
                }
                encoding.AddPremise(encoding.MakeOr(post));

                // add past predicate
                result->Conjoin(std::make_unique<FuturePredicate>(std::move(futPreMem), std::move(futPostMem), std::move(futContext->now)));
            }
        }
    }

    inline void DeriveJoinedStack() {
        MEASURE("Solver::Join ~> DeriveJoinedStack")
        DEBUG("[join] starting stack extension..." << std::flush)
        plankton::ExtendStack(*result, encoding, EXTENSION);
        DEBUG(" done" << std::endl)
    }
};

std::unique_ptr<Annotation> Solver::Join(std::deque<std::unique_ptr<Annotation>> annotations) const {
    MEASURE("Solver::Join")
    DEBUG(std::endl << std::endl << "=== joining " << annotations.size() << std::endl)
    for (const auto& elem : annotations) DEBUG(*elem)
    DEBUG(std::endl)
    
    if (annotations.empty()) throw std::logic_error("Cannot join empty set"); // TODO: better error handling
    if (annotations.size() == 1) return std::move(annotations.front());
    {
        // TODO: needed for Past-to-Past Interpolation in the case 'annotations.size() == 1' as well?
        // TODO: needed?
        MEASURE("Solver::Join ~> ImprovePast")
        for (auto& elem : annotations) ImprovePast(*elem);
        for (auto& elem : annotations) PrunePast(*elem);
    }
    auto result = AnnotationJoiner(std::move(annotations), config).GetResult();
    plankton::InlineAndSimplify(*result);
    // PrunePast(*result);
     DEBUG("** join result: " << *result << std::endl << std::endl << std::endl)
//    DEBUG(*result << std::endl << std::endl)
    
    // if (!result->past.empty()) throw std::logic_error("--- breakpoint ---");
    return result;
}