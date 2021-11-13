#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"

using namespace plankton;

constexpr const std::size_t MAX_JOIN = 5;
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
    }
};

template<typename I>
inline std::deque<std::vector<typename I::pointer>>
MakePowerSet(std::vector<AnnotationInfo>& lookup, const std::function<std::pair<I, I>(AnnotationInfo&)>& getIterators) {
    std::vector<I> begin, end;
    begin.reserve(lookup.size());
    end.reserve(lookup.size());
    bool containsEmpty = false;
    for (auto& info : lookup) {
        auto [infoBegin, infoEnd] = getIterators(info);
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
            else {
                isEnd = false;
                break;
            }
        }
        return !isEnd;
    };
    // auto next = [&cur,&begin,&end]() -> bool {
    //     bool isEnd = true;
    //     for (std::size_t index = 0; index < cur.size(); ++index) {
    //         auto& it = cur[index];
    //         it.operator++();
    //         if (it == end[index]) it = begin[index];
    //         else isEnd = false;
    //     }
    //     return !isEnd;
    // };

    // create power set
    std::deque<std::vector<typename I::pointer>> result;
    do {
        result.emplace_back();
        result.back().reserve(cur.size());
        for (auto it : cur) result.back().push_back(&*it);
    } while (next());
    return result;
}

inline auto MakePastPowerSet(std::vector<AnnotationInfo>& lookup, const VariableDeclaration& variable) {
    return MakePowerSet<std::set<const PastPredicate*>::const_iterator>(lookup, [&variable](auto& info) {
        auto& set = info.varToHist[&variable];
        return std::make_pair(set.begin(), set.end());
    });
}

inline auto MakeFuturePowerSet(std::vector<AnnotationInfo>& lookup) {
    return MakePowerSet<std::deque<std::unique_ptr<FuturePredicate>>::const_iterator>(lookup, [](auto& info) {
        auto& future = info.annotation.future;
        return std::make_pair(future.begin(), future.end());
    });
}

// inline bool FuturesMatch(const FuturePredicate& future, const FuturePredicate& other, Encoding& encoding) { // FuturePremiseMatches
//     if (!plankton::SyntacticalEqual(*future.guard, *other.guard)) return false;
//     if (future.update->fields.size() != other.update->fields.size()) return false;
//     for (std::size_t index = 0; index < future.update->fields.size(); ++index) {
//         if (!plankton::SyntacticalEqual(*future.update->fields.at(index), *other.update->fields.at(index))) return false;
//     }
//     for (std::size_t index = 0; index < future.update->values.size(); ++index) {
//         auto equal = encoding.Encode(*future.update->values.at(index)) == encoding.Encode(*other.update->values.at(index));
//         if (!encoding.Implies(equal)) return false;
//     }
//     return true;
// }

inline EExpr FuturesMatch(const FuturePredicate& future, const FuturePredicate& other, Encoding& encoding) {
    if (!plankton::SyntacticalEqual(*future.guard, *other.guard)) return encoding.Bool(false);
    if (future.update->fields.size() != other.update->fields.size()) return encoding.Bool(false);
    for (std::size_t index = 0; index < future.update->fields.size(); ++index) {
        if (!plankton::SyntacticalEqual(*future.update->fields.at(index), *other.update->fields.at(index))) return encoding.Bool(false);
    }
    auto equalities = plankton::MakeVector<EExpr>(future.update->values.size());
    for (std::size_t index = 0; index < future.update->values.size(); ++index) {
        auto equal = encoding.Encode(*future.update->values.at(index)) == encoding.Encode(*other.update->values.at(index));
        equalities.push_back(equal);
    }
    return encoding.MakeAnd(equalities);
}

inline void AddResourceCopies(Annotation& annotation, const SymbolDeclaration& src, const SymbolDeclaration& dst) {
    annotation.Conjoin(MakeEq(src, dst));
    auto renaming = [&src, &dst](const auto& decl) -> const SymbolDeclaration& { return decl == src ? dst : decl; };

    // copy memory
    if (auto mem = plankton::TryGetResource(src, *annotation.now)) {
        auto newMem = plankton::Copy(*mem);
        plankton::RenameSymbols(*newMem, renaming);
        assert(newMem->node->Decl() == dst);
        annotation.Conjoin(std::move(newMem));
    }

    // copy pasts
    for (const auto& past : annotation.past) {
        if (past->formula->node->Decl() != src) continue;
        auto newPast = plankton::Copy(*past);
        plankton::RenameSymbols(*newPast, renaming);
        assert(newPast->formula->node->Decl() == dst);
        annotation.Conjoin(std::move(newPast));
    }

    // copy futures // TODO: should no longer be needed!?
    for (const auto& future : annotation.future) {
        auto containsSrc = plankton::Any(future->update->values, [&src](const auto& value){
            if (auto var = dynamic_cast<const SymbolicVariable*>(value.get())) return var->Decl() == src;
            return false;
        });
        if (!containsSrc) continue;
        auto newFuture = plankton::Copy(*future);
        plankton::RenameSymbols(*newFuture, renaming);
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
        Preprocess();
        
        DEBUG("(preprocessed)" << std::endl)
        for (const auto& elem : annotations) DEBUG(*elem)
        DEBUG(std::endl)

        CreateVariableResources();
        CreateMemoryResources();
        CreateObligationResources();
        CreateFulfillmentResources();
        EncodeMatching();
        EncodeNow();
        EncodePast();
        EncodeFuture();
        DeriveJoinedStack();

        return std::move(result);
    }

    inline void Preprocess() {
        // DEBUG("[join] starting preprocessing..." << std::flush)
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
                plankton::ExtendStack(*annotation, config, EXTENSION);
            }

            // distinct symbols across annotations
            plankton::RenameSymbols(*annotation, factory);
            
            // distinct values across variables // TODO: is this really necessary??? <<<<<<<<<========================================||||||
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
        // DEBUG(" done" << std::endl)
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

    inline EExpr EncodeAnnotation(const AnnotationInfo& info) {
        auto enc = plankton::MakeVector<EExpr>(varToCommonRes.size() + varToCommonMem.size() + 1);

        // state & invariants
        auto state = encoding.EncodeFormulaWithKnowledge(*info.annotation.now, config);
        enc.push_back(state);

        // force common variables to agree
        for (const auto& [var, resource] : varToCommonRes) {
            auto other = info.varToRes.at(var);
            enc.push_back(encoding.Encode(resource->Value()) == encoding.Encode(other->Value()));
        }

        // force common memory to agree
        for (const auto& [var, memory] : varToCommonMem) {
            auto other = info.varToMem.at(var);
            enc.push_back(encoding.EncodeMemoryEquality(*memory, *other));
        }

        return encoding.MakeAnd(enc);
    }

    inline void EncodeMatching() {
        for (auto& info : lookup) {
            for (const auto& [var, memory] : varToCommonMem) {
                auto adr = encoding.Encode(memory->node->Decl());
                auto other = encoding.Encode(info.varToMem.at(var)->node->Decl());
                encoding.AddPremise(adr == other);
            }
        }

        encoding.AddPremise(encoding.Encode(*result->now));
        assert(!encoding.ImpliesFalse());
    }

    inline void EncodeNow() {
        auto disjunction = plankton::MakeVector<EExpr>(lookup.size());
        for (const auto& info : lookup) {
            disjunction.push_back(EncodeAnnotation(info));
        }
        encoding.AddPremise(encoding.MakeOr(disjunction));
        assert(!encoding.ImpliesFalse());
    }

    inline void EncodePast() {
        for (const auto&[variable, resource] : varToCommonRes) {
            auto powerSet = MakePastPowerSet(lookup, *variable);
            for (const auto& vector : powerSet) {
                // DEBUG(" join past combination:  ")
                // for (const auto* elem : vector) DEBUG(**elem << ",  ")
                // DEBUG(std::endl)

                auto& address = resource->Value();
                auto pastMem = plankton::MakeSharedMemory(address, config.GetFlowValueType(), factory);

                // force common past memory to agree
                auto encoded = plankton::MakeVector<EExpr>(vector.size());
                for (std::size_t infoIndex = 0; infoIndex < vector.size(); ++infoIndex) {
                    auto memEq = encoding.EncodeMemoryEquality(*pastMem, *(**vector.at(infoIndex)).formula);
                    auto context = EncodeAnnotation(lookup.at(infoIndex));
                    encoded.push_back(memEq && context);
                }
                encoding.AddPremise(encoding.MakeOr(encoded));

                // add past predicate
                result->Conjoin(std::make_unique<PastPredicate>(std::move(pastMem)));
            }
        }
        assert(!encoding.ImpliesFalse());
    }

    inline void EncodeFuture() {
        MEASURE("Solver::Join ~> FutureMatching")
        DEBUG("(starting future matching...)" << std::endl)
        auto powerSet = MakeFuturePowerSet(lookup);

        // prune mismatches
        for (auto& vector : powerSet) {
            if (vector.empty()) continue;
            auto& cmp = **vector.front();
            auto checks = plankton::MakeVector<EExpr>(vector.size());
            for (const auto elem : vector) checks.push_back(FuturesMatch(cmp, **elem, encoding));
            encoding.AddCheck(encoding.MakeAnd(checks), [&vector](bool holds) {
                if (!holds) vector.clear();
            });
        }
        encoding.Check();

        // add joined futures
        for (const auto& vector : powerSet) {
            if (vector.empty()) continue;

            // make future
            auto future = plankton::Copy(**vector.front());
            for (auto& value : future->update->values) {
                value = std::make_unique<SymbolicVariable>(factory.GetFresh(value->Type(), value->Order()));
            }

            // force update to agree
            for (std::size_t index = 0; index < future->update->values.size(); ++index) {
                auto val = encoding.Encode(*future->update->values.at(index));
                auto equalities = plankton::MakeVector<EExpr>(vector.size());
                for (std::size_t infoIndex = 0; infoIndex < vector.size(); ++infoIndex) {
                    auto equal = val == encoding.Encode(*(**vector.at(infoIndex)).update->values.at(index));
                    auto context = EncodeAnnotation(lookup.at(infoIndex));
                    equalities.push_back(equal && context);
                }
                encoding.AddPremise(encoding.MakeOr(equalities));
            }

            result->Conjoin(std::move(future));
        }
        assert(!encoding.ImpliesFalse());
    }

    inline void DeriveJoinedStack() {
        MEASURE("Solver::Join ~> DeriveJoinedStack")
        DEBUG("(starting stack extension...)" << std::endl)
        plankton::ExtendStack(*result, encoding, EXTENSION);
        // DEBUG(" done" << std::endl)
    }
};

std::unique_ptr<Annotation> Solver::Join(std::deque<std::unique_ptr<Annotation>> annotations) const {
    MEASURE("Solver::Join")
    DEBUG("<<JOIN>> " << annotations.size() << std::endl)
    if (annotations.empty()) throw std::logic_error("Cannot join empty set"); // TODO: better error handling
    if (annotations.size() == 1) return std::move(annotations.front());

    while (annotations.size() > MAX_JOIN) {
        decltype(annotations) part;
        for (std::size_t index = 0; index < MAX_JOIN; ++index) {
            assert(!annotations.empty());
            part.push_back(std::move(annotations.front()));
            annotations.pop_front();
        }
        assert(part.size() <= MAX_JOIN);
        DEBUG("joining subset first" << std::endl)
        auto join = Join(std::move(part));
        ReducePast(*join);
        ReduceFuture(*join);
        annotations.push_back(std::move(join));
    }

    auto result = AnnotationJoiner(std::move(annotations), config).GetResult();
    plankton::InlineAndSimplify(*result);
    DEBUG("== join result: " << *result << std::endl << std::endl << std::endl)
    return result;
}