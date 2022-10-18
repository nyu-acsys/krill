#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/flowgraph.hpp"
#include "engine/util.hpp"
#include "util/log.hpp"
#include "util/timer.hpp"


using namespace plankton;


// NOTE:
// This checks for acyclicity of the flow graph. One would prefer to check for non-empty flow cycles instead.
// However, this would require to construct many flow graphs when finding new points-to predicates.
// Hence, we go with a syntactic check for reachability.


struct PostImageInfo {
    const SolverConfig& config;
    const MemoryWrite& command;
    FlowGraph footprint;
    Encoding encoding;
    Annotation& pre;
    std::set<const ObligationAxiom*> preObligations;
    std::set<const FulfillmentAxiom*> preFulfillments;
    std::deque<std::unique_ptr<Axiom>> postSpecifications;
    std::optional<bool> isPureUpdate = std::nullopt;
    
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> outsideInsideAlias;
    std::map<const SymbolDeclaration*, std::set<const SymbolDeclaration*>> outsideInsideDistinct;
    std::map<const SymbolDeclaration*, std::deque<std::unique_ptr<Axiom>>> effectContext;
    
    explicit PostImageInfo(std::unique_ptr<Annotation> pre_, const MemoryWrite& cmd, const SolverConfig& config)
            : config(config), command(cmd), footprint(plankton::MakeFlowFootprint(std::move(pre_), cmd, config)),
              encoding(footprint), pre(*footprint.pre),
              preObligations(plankton::Collect<ObligationAxiom>(*pre.now)),
              preFulfillments(plankton::Collect<FulfillmentAxiom>(*pre.now)) {
        assert(&pre == footprint.pre.get());
        // DEBUG("** pre after footprint creation: " << pre << std::endl)
    }
    
    template<typename T, typename = plankton::EnableIfBaseOf<MemoryAxiom, T>>
    inline std::set<const T*> GetOutsideMemory() {
        return plankton::Collect<T>(*pre.now, [this](const auto& memory){
            return footprint.GetNodeOrNull(memory.node->Decl()) == nullptr;
        });
    }
};

template<BinaryOperator Op>
inline std::unique_ptr<StackAxiom> MakeBinary(const SymbolDeclaration& lhs, const SymbolDeclaration& rhs) {
    return std::make_unique<StackAxiom>(Op, std::make_unique<SymbolicVariable>(lhs),
                                        std::make_unique<SymbolicVariable>(rhs));
}

template<BinaryOperator Op>
inline std::unique_ptr<StackAxiom> MakeBinary(const SymbolDeclaration& lhs, std::unique_ptr<SymbolicExpression> rhs) {
    return std::make_unique<StackAxiom>(Op, std::make_unique<SymbolicVariable>(lhs), std::move(rhs));
}


//
// Checks
//

// inline void CheckPublishing(PostImageInfo& info) {
//     for (auto& node : info.footprint.nodes) {
//         if (node.preLocal == node.postLocal) continue;
//         node.needed = true;
//         for (auto& field : node.pointerFields) {
//             auto next = info.footprint.GetNodeOrNull(field.postValue);
//             if (!next) throw std::logic_error("Footprint too small to capture publishing"); // TODO: better error handling
//             next->needed = true;
//         }
//     }
// }

inline void CheckPublishing(PostImageInfo& info) {
    for (auto& node : info.footprint.nodes) {
        if (node.preLocal == node.postLocal) continue;
        node.needed = true;
        for (auto& field : node.pointerFields) {
            if (info.footprint.GetNodeOrNull(field.postValue)) continue;
            if (auto next = plankton::TryGetResource(field.postValue, *info.pre.now)) {
                if (dynamic_cast<const SharedMemoryCore*>(next)) continue;
            }
            info.encoding.AddCheck(info.encoding.EncodeIsNull(field.postValue), [](bool holds){
                if (holds) return;
                throw std::logic_error("Footprint too small to capture publishing"); // TODO: better error handling
            });
        }
    }
}

inline void CheckReachability(PostImageInfo& info) {
    auto preReach = plankton::ComputeReachability(info.footprint, EMode::PRE);
    auto postReach = plankton::ComputeReachability(info.footprint, EMode::POST);

    // ensure no cycle inside footprint after update
    for (const auto& node : info.footprint.nodes) {
        if (!postReach.IsReachable(node.address, node.address)) continue;
        throw std::logic_error("Update failed: cycle withing footprint detected."); // TODO: better error handling
    }
    
    // ensure that no nodes outside the footprint are reachable after the update that were unreachable before
     auto notUpdatedOrShared = [&info](const auto& field) {
         if (!field.HasUpdated()) return true;
         if (auto node = info.footprint.GetNodeOrNull(field.postValue)) return !node->preLocal;
         return false;
     };
     for (const auto& node : info.footprint.nodes) {
         if (!node.HasUpdatedPointers()) continue;
         if (node.preLocal && plankton::All(node.pointerFields, notUpdatedOrShared)) continue;

         for (const auto* reach : postReach.GetReachable(node.address)) {
             if (preReach.IsReachable(node.address, *reach)) continue;
             if (node.preLocal) {
                 if (auto reachNode = info.footprint.GetNodeOrNull(*reach)) {
                     if (!reachNode->preLocal) continue;
                 }
             }

             auto vec = plankton::MakeVector<EExpr>(8);
             auto& nodeReach = preReach.GetReachable(node.address);
             for (const auto* elem : preReach.GetReachable(*reach)) {
                 if (plankton::Membership(nodeReach, elem)) continue;
                 vec.push_back(info.encoding.EncodeIsNull(*elem));
             }
             auto missingReachIsNull = info.encoding.MakeAnd(vec);

             info.encoding.AddCheck(missingReachIsNull || info.encoding.EncodeIsNull(*reach), [reach,&node](bool holds){
                 if (holds) return;
                 throw std::logic_error("Update failed: cannot guarantee acyclicity via potentially non-null non-footprint address " + reach->name + " from address " + node.address.name + "."); // TODO: better error handling
             });

             // if (plankton::Subset(preReach.GetReachable(*reach), preReach.GetReachable(node.address)))
             //     continue;
             // info.encoding.AddCheck(info.encoding.EncodeIsNull(*reach), [reach,&node](bool holds){
             //     if (holds) return;
             //     throw std::logic_error("Update failed: cannot guarantee acyclicity via potentially non-null non-footprint address " + reach->name + " from address " + node.address.name + "."); // TODO: better error handling
             // });
         }
     }
}

inline void CheckFlowCoverage(PostImageInfo& info) {
    auto ensureContains = [&info](const SymbolDeclaration& address){
        auto mustHaveNode = info.footprint.GetNodeOrNull(address);
        if (!mustHaveNode) throw std::logic_error("Update failed: footprint does not cover addresses " + address.name +
                                                  " the inflow of which changed."); // TODO: better error handling
        mustHaveNode->needed = true;
    };

    for (const auto& node : info.footprint.nodes) {
        for (const auto& field : node.pointerFields) {
            auto sameFlow = info.encoding.Encode(field.preAllOutflow) == info.encoding.Encode(field.postAllOutflow);
            auto isPreNull = info.encoding.EncodeIsNull(field.preValue);
            auto isPostNull = info.encoding.EncodeIsNull(field.postValue);
            if (!info.encoding.Implies(sameFlow || isPreNull)) ensureContains(field.preValue);
            if (!info.encoding.Implies(sameFlow || isPostNull)) ensureContains(field.postValue);
        }
    }
}

inline void CheckFlowUniqueness(PostImageInfo& info) {
    auto disjointness = info.encoding.EncodeKeysetDisjointness(info.footprint, EMode::POST);
    if (!info.encoding.Implies(disjointness)) {
        throw std::logic_error("Unsafe update: keyset disjointness not guaranteed."); // TODO: better error handling
    }

    auto uniqueness = info.encoding.EncodeInflowUniqueness(info.footprint, EMode::POST);
    if (!info.encoding.Implies(uniqueness)) {
        throw std::logic_error("Unsafe update: inflow uniqueness not guaranteed."); // TODO: better error handling
    };
}

inline void CheckInvariant(PostImageInfo& info) {
    for (const auto& node : info.footprint.nodes) {
        auto nodeInvariant = info.encoding.EncodeNodeInvariant(node, EMode::POST);

        // // // DEBUG
        // if (!node.postLocal) {
        //     auto theNodeRaw = node.ToLogic(EMode::POST);
        //     auto theNode = dynamic_cast<const SharedMemoryCore*>(theNodeRaw.get());
        //     assert(theNode);
        //     DEBUG("~chk inv for: " << *theNode << std::endl)
        //     auto inv = info.config.GetSharedNodeInvariant(*theNode);
        //     for (const auto& elem : inv->conjuncts) {
        //         auto holds = info.encoding.Implies(*elem);
        //         DEBUG("   - " << holds << ": " << *elem << std::endl)
        //     }
        // }

        info.encoding.AddCheck(nodeInvariant, [&node](bool holds){
            if (holds) return;
            throw std::logic_error("Unsafe update: invariant is not maintained for node '" + node.address.name + "'."); // TODO: better error handling
        });
    }
}

inline void AddSpecificationChecks(PostImageInfo& info) {
    // check purity
    plankton::AddPureCheck(info.footprint, info.encoding, [&info](bool isPure) {
        info.isPureUpdate = isPure;
        if (isPure) {
            for (const auto* obl : info.preObligations) info.postSpecifications.push_back(plankton::Copy(*obl));
            for (const auto* ful : info.preFulfillments) info.postSpecifications.push_back(plankton::Copy(*ful));
            return;
        }
        if (!info.preObligations.empty()) return;
        throw std::logic_error("Unsafe update: impure update without obligation."); // TODO: better error handling
    });

    // check impurity and obligations
    for (const auto* obligation : info.preObligations){
        plankton::AddPureSpecificationCheck(info.footprint, info.encoding, *obligation, [&info](auto ful) {
            if (!ful.has_value()) return;
            info.postSpecifications.push_back(std::make_unique<FulfillmentAxiom>(ful.value()));
        });
        plankton::AddImpureSpecificationCheck(info.footprint, info.encoding, *obligation, [&info](auto ful) {
            if (ful.has_value()) {
                info.postSpecifications.push_back(std::make_unique<FulfillmentAxiom>(ful.value()));
                return;
            }
            assert(info.isPureUpdate.has_value());
            if (info.isPureUpdate.value_or(false)) return;
            throw std::logic_error("Unsafe update: impure update that does not satisfy the specification");
        });
    }
}

inline void AddAffectedOutsideChecks(PostImageInfo& info) {
    auto outsideMemory = info.GetOutsideMemory<SharedMemoryCore>();
    for (const auto& inside : info.footprint.nodes) {
        auto& insideAdr = inside.address;
        for (const auto* outside : outsideMemory) {
            auto& outsideAdr = outside->node->Decl();
            auto isAlias = info.encoding.Encode(insideAdr) == info.encoding.Encode(outsideAdr);
            auto isDistinct = info.encoding.Encode(insideAdr) != info.encoding.Encode(outsideAdr);
            info.encoding.AddCheck(isAlias, [&info,in=&insideAdr,out=&outsideAdr](bool holds) {
                if (!holds) return;
                info.outsideInsideAlias.emplace(out, in);
                info.pre.Conjoin(MakeBinary<BinaryOperator::EQ>(*in, *out));
            });
            info.encoding.AddCheck(isDistinct, [&info,in=&insideAdr,out=&outsideAdr](bool holds) {
                if (!holds) return;
                info.outsideInsideDistinct[out].insert(in);
                info.pre.Conjoin(MakeBinary<BinaryOperator::NEQ>(*in, *out));
            });
        }
    }
}


//
// Minimization
//

inline void MinimizeFootprint(PostImageInfo& info) {
    // plankton::RemoveIf(info.footprint.nodes, [](const auto& node){ return !node.needed; }); // does not work
    std::deque<FlowGraphNode> nodes;
    for (auto& node : info.footprint.nodes) {
        if (!node.needed && !node.HasUpdated()) continue;
        nodes.push_back(std::move(node));
    }
    info.footprint.nodes = std::move(nodes);
}


//
// Post Image
//

inline std::unique_ptr<SharedMemoryCore> HandleSharedOutside(PostImageInfo& info, const SharedMemoryCore& outside) {
    auto& outsideAdr = outside.node->Decl();
    auto result = plankton::Copy(outside);
    if (auto alias = info.outsideInsideAlias[&outsideAdr]) {
        result->node->decl = *alias;
        return result;
    }
    
    SymbolFactory factory(info.pre);
    auto updateFlow = [&result,&factory](){
        result->flow->decl = factory.GetFresh(result->flow->GetType(), result->flow->GetOrder());
    };
    auto updateField = [&result,&factory](const Field& field){
        auto& value = result->fieldToValue.at(field.name);
        value->decl = factory.GetFresh(value->GetType(), value->GetOrder());
    };
    
    for (const auto& inside : info.footprint.nodes) {
        if (plankton::Membership(info.outsideInsideDistinct[&outsideAdr], &inside.address)) continue;
        if (inside.HasUpdatedFlow()) updateFlow();
        for (const auto& field : inside.dataFields) if (field.HasUpdated()) updateField(field);
        for (const auto& field : inside.pointerFields) if (field.HasUpdated()) updateField(field);
    }

    return result;
}

inline std::unique_ptr<Annotation> ExtractPost(PostImageInfo&& info) {
    // create post memory
    std::deque<std::unique_ptr<MemoryAxiom>> newMemory;
    auto outsideLocal = info.GetOutsideMemory<LocalMemoryResource>();
    auto outsideShared = info.GetOutsideMemory<SharedMemoryCore>();
    for (const auto& node : info.footprint.nodes) {
        newMemory.push_back(node.ToLogic(EMode::POST));
    }
    for (const auto* local : outsideLocal) {
        newMemory.push_back(plankton::Copy(*local));
    }
    for (const auto* shared : outsideShared) {
        newMemory.push_back(HandleSharedOutside(info, *shared));
    }
    
    // delete old memory/obligations/fulfillments
    struct : public LogicListener {
        bool result = false;
        void Enter(const LocalMemoryResource& /*object*/) override { result = true; }
        void Enter(const SharedMemoryCore& /*object*/) override { result = true; }
        void Enter(const ObligationAxiom& /*object*/) override { result = true; }
        void Enter(const FulfillmentAxiom& /*object*/) override { result = true; }
        bool Delete(const LogicObject& object) { result = false; object.Accept(*this); return result; }
    } check;
    plankton::Simplify(*info.pre.now);
    info.pre.now->RemoveConjunctsIf([&check](const auto& elem){ return check.Delete(elem); });
    assert(plankton::Collect<MemoryAxiom>(*info.pre.now).empty());
    assert(plankton::Collect<ObligationAxiom>(*info.pre.now).empty());
    assert(plankton::Collect<FulfillmentAxiom>(*info.pre.now).empty());
    
    // put together post
    plankton::MoveInto(std::move(newMemory), info.pre.now->conjuncts);
    plankton::MoveInto(std::move(info.postSpecifications), info.pre.now->conjuncts);
    return std::move(info.footprint.pre);
}


//
// Extract Effects
//

inline void AddEffectPrecisionCheck(PostImageInfo& info) {
    for (auto& node : info.footprint.nodes) {
        auto preFlow = info.encoding.Encode(node.preAllInflow);
        auto postFlow = info.encoding.Encode(node.postAllInflow);
        info.encoding.AddCheck(preFlow == postFlow, [&node](bool holds){
            if (holds) node.postAllInflow = node.preAllInflow;
        });
    }
}

inline std::vector<std::function<std::unique_ptr<Axiom>()>> GetContextGenerators(const SymbolDeclaration& symbol) {
    switch (symbol.order) {
        case Order::FIRST:
            switch (symbol.type.sort) {
                case Sort::VOID: return {};
                case Sort::BOOL: return { // true or false
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicBool>(true)); },
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicBool>(false)); }
                };
                case Sort::DATA: return { // min, max, or between
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicMin>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::GT>(symbol, std::make_unique<SymbolicMin>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicMax>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::LT>(symbol, std::make_unique<SymbolicMax>()); }
                };
                case Sort::TID: return { // NULL or not
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicSelfTid>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::NEQ>(symbol, std::make_unique<SymbolicSelfTid>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicSomeTid>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicUnlocked>()); }
                };
                case Sort::PTR: return { // NULL or not
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicNull>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::NEQ>(symbol, std::make_unique<SymbolicNull>()); }
                };
            }
            throw;
        case Order::SECOND:
            return { // empty or not
                [&symbol]() { return std::make_unique<InflowEmptinessAxiom>(symbol, true); },
                [&symbol]() { return std::make_unique<InflowEmptinessAxiom>(symbol, false); }
            };
    }
    throw;
}

inline void AddEffectContextGenerators(PostImageInfo& info) {
    // collect variables appearing in footprint
    std::set<const SymbolDeclaration*> symbols;
    for (const auto& node : info.footprint.nodes) {
        // auto dummy = node.ToLogic(EMode::PRE); // TODO: avoid construction
        // plankton::InsertInto(plankton::Collect<SymbolDeclaration>(*dummy), symbols);
        plankton::InsertInto(plankton::Collect<SymbolDeclaration>(*node.ToLogic(EMode::PRE)), symbols);
        plankton::InsertInto(plankton::Collect<SymbolDeclaration>(*node.ToLogic(EMode::POST)), symbols);
    }

    for (const auto* decl : symbols) {
        for (auto&& generator : GetContextGenerators(*decl)) {
            auto candidate = info.encoding.Encode(*generator());
            info.encoding.AddCheck(candidate, [decl,generator,&info](bool holds){
                if (holds) info.effectContext[decl].push_back(generator());
            });
        }
    }
}

inline std::unique_ptr<Formula> GetNodeUpdateContext(PostImageInfo& info, const MemoryAxiom& pre, const MemoryAxiom& post) {
    auto result = std::make_unique<SeparatingConjunction>();
    auto handle = [&result,&info](const SymbolDeclaration& decl) {
        for (const auto& elem : info.effectContext[&decl]) result->Conjoin(plankton::Copy(*elem));
    };
    std::set<const SymbolDeclaration*> symbols;
    for (const auto* node : {&pre, &post}) {
        symbols.insert(&node->flow->Decl());
        for (const auto& pair: node->fieldToValue) symbols.insert(&pair.second->Decl());
    }
    for (const auto* symbol : symbols) {
        handle(*symbol);
    }
    plankton::Simplify(*result);
    return result;
}

inline std::unique_ptr<SharedMemoryCore> ToSharedMemoryCore(const FlowGraphNode& node, EMode mode) {
    auto logic = node.ToLogic(mode);
    if (auto shared = dynamic_cast<SharedMemoryCore*>(logic.get())) {
        (void) logic.release(); // ignore return value; squelch warning with cast
        return std::unique_ptr<SharedMemoryCore>(shared);
    }
    throw std::logic_error("Internal error: cannot produce shared memory");
}

inline std::unique_ptr<HeapEffect> ExtractEffect(PostImageInfo& info, const FlowGraphNode& node) {
    // ignore local nodes; after minimization, all remaining footprint nodes contain changes
    if (node.preLocal) return nullptr;
    if (!node.HasUpdated()) return nullptr;
    auto pre = ToSharedMemoryCore(node, EMode::PRE);
    auto post = ToSharedMemoryCore(node, EMode::POST);
    auto context = GetNodeUpdateContext(info, *pre, *post);
    return std::make_unique<HeapEffect>(std::move(pre), std::move(post), std::move(context));
}

inline std::deque<std::unique_ptr<HeapEffect>> ExtractEffects(PostImageInfo& info) {
    // create a separate effect for every node
    std::deque<std::unique_ptr<HeapEffect>> result;
    for (const auto& node : info.footprint.nodes) {
        auto effect = ExtractEffect(info, node);
        if (!effect) continue;
        assert(effect);
        assert(effect->pre);
        assert(effect->post);
        assert(effect->context);
        result.push_back(std::move(effect));
        //DEBUG("EFF: " << *result.back() << std::endl)
    }
    return result;
}


//
// Search for future
//

inline std::unique_ptr<Update> MakeUpdate(const Formula& state, const MemoryWrite& cmd) {
    assert(cmd.lhs.size() == cmd.rhs.size());
    auto result = std::make_unique<Update>();
    for (std::size_t index = 0; index < cmd.lhs.size(); ++index) {
        auto value = plankton::TryMakeSymbolic(*cmd.rhs.at(index), state);
        if (!value) return nullptr;
        result->fields.push_back(plankton::Copy(*cmd.lhs.at(index)));
        result->values.push_back(std::move(value));
    }
    return result;
}

inline std::optional<EExpr> MakeEnablednessCheck(Encoding& encoding, const FuturePredicate& future, const Update& cmd, const Formula& state) {
    assert(future.update->fields.size() == future.update->values.size());

    // check updated fields are syntactically equal
    if (future.update->fields.size() != cmd.fields.size()) return std::nullopt;
    for (std::size_t index = 0; index < cmd.fields.size(); ++index) {
        auto sameField = plankton::SyntacticalEqual(*future.update->fields.at(index), *cmd.fields.at(index));
        if (!sameField) return std::nullopt;
    }

    // check performed updates are semantically equal
    auto conditions = plankton::MakeVector<EExpr>(future.guard->conjuncts.size() + future.update->fields.size());
    for (std::size_t index = 0; index < cmd.fields.size(); ++index) {
        auto sameValue = encoding.Encode(*future.update->values.at(index)) == encoding.Encode(*cmd.values.at(index));
        conditions.push_back(sameValue);
    }

    // check guard is enabled
    for (const auto& guard : future.guard->conjuncts) {
        auto lhs = plankton::MakeSymbolic(*guard->lhs, state);
        auto rhs = plankton::MakeSymbolic(*guard->rhs, state);
        if (!lhs || !rhs) return encoding.Bool(false);
        StackAxiom dummy(guard->op, std::move(lhs), std::move(rhs));
        conditions.push_back(encoding.Encode(dummy));
    }

    return encoding.MakeAnd(conditions);
}

inline bool CoveredByFuture(const Annotation& pre, const Update& update) {
    auto& now = *pre.now;
    Encoding encoding(now);
    bool eureka = false;
    for (const auto& future : pre.future) {
        auto enabled = MakeEnablednessCheck(encoding, *future, update, now);
        if (!enabled) continue;
        encoding.AddCheck(*enabled, [&eureka](bool holds){
            if (holds) eureka = true;
        });
    }
    encoding.Check();
    return eureka;
}

inline std::map<SharedMemoryCore*, std::set<std::string>> GetAliasChanges(Annotation& annotation, const Update& update) {
    std::map<SharedMemoryCore*, std::set<std::string>> result;
    Encoding encoding(*annotation.now);
    // TODO: add invariants and simple flow rules to encoding?
    auto memories = plankton::CollectMutable<SharedMemoryCore>(*annotation.now);
    for (auto* memory : memories) {
        auto adr = encoding.Encode(memory->node->Decl());
        for (const auto& deref : update.fields) {
            auto notAffected = adr != encoding.Encode(deref->variable->Decl());
            encoding.AddCheck(notAffected, [memory,&deref,&result](bool holds){
                if (holds) return;
                result[memory].insert(deref->fieldName);
            });
        }
    }
    encoding.Check();
    return result;
}

inline std::unique_ptr<StackAxiom> MakeEq(const SymbolDeclaration& decl, const SymbolicExpression& expr) {
    return std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(decl), plankton::Copy(expr));
}

std::unique_ptr<Annotation> TryGetFromFuture(const Annotation& pre, const MemoryWrite& cmd) {
    auto update = MakeUpdate(*pre.now, cmd);
    if (!update) return nullptr;
    if (!CoveredByFuture(pre, *update)) return nullptr;

    auto result = plankton::Copy(pre);
    SymbolFactory factory(*result);

    // update all flow fields and fields potentially affected by the update
    auto changeField = [&factory](auto& decl){
        decl = factory.GetFresh(decl.get().type, decl.get().order);
    };
    for (auto* memory : plankton::CollectMutable<SharedMemoryCore>(*result->now)) {
        changeField(memory->flow->decl);
    }
    for (const auto& [memory, changedFields] : GetAliasChanges(*result, *update)) {
        for (const auto& field : changedFields) changeField(memory->fieldToValue.at(field)->decl);
    }

    // perform update
    for (std::size_t index = 0; index < update->fields.size(); ++index) {
        auto& field = *update->fields.at(index);
        auto& value = *update->values.at(index);
        auto& variable = plankton::GetResource(field.variable->Decl(), *result->now);
        auto& memory = plankton::GetResource(variable.Value(), *result->now);
        auto& newValue = factory.GetFreshFO(value.GetType());
        memory.fieldToValue.at(field.fieldName)->decl = newValue;
        result->Conjoin(MakeEq(newValue, value));
    }

    plankton::InlineAndSimplify(*result);
    return result;
}

//
// Overall Algorithm
//

inline bool IsTrivial(const Formula& state, const MemoryWrite& cmd) {
    Encoding encoding(state);
    auto equalities = plankton::MakeVector<EExpr>(cmd.lhs.size());
    for (std::size_t index = 0; index < cmd.lhs.size(); ++index) {
        auto* cur = plankton::TryEvaluate(*cmd.lhs.at(index), state);
        auto dst = plankton::TryMakeSymbolic(*cmd.rhs.at(index), state);
        if (!cur || !dst) return false;
        equalities.push_back(encoding.Encode(*cur) == encoding.Encode(*dst));
    }
    return encoding.Implies(encoding.MakeAnd(equalities));
}

void WriteGraphToFile(const FlowGraph&, const std::shared_ptr<EngineSetup>&, const MemoryWrite&, bool);

PostImage Solver::Post(std::unique_ptr<Annotation> pre, const MemoryWrite& cmd, bool useFuture) const {
    MEASURE("Solver::Post (MemoryWrite)")
    DEBUG("<<POST MEM>> [useFuture=" << useFuture << "]" << std::endl << *pre << " " << cmd << std::flush)
    if (IsUnsatisfiable(*pre)) {
        DEBUG("{ false }" << std::endl << std::endl)
        return PostImage();
    }

    PrepareAccess(*pre, cmd);
    plankton::InlineAndSimplify(*pre);
    if (IsTrivial(*pre->now, cmd)) return PostImage(std::move(pre)); // TODO: needed?

    // TODO: use futures as a last resort their post image is less precise
    std::unique_ptr<Annotation> fromFuture = nullptr;
    if (useFuture && !pre->future.empty()) {
        fromFuture = TryGetFromFuture(*pre, cmd);
    }

    try {
        PostImageInfo info(std::move(pre), cmd, config);
        if (info.encoding.ImpliesFalse()) return PostImage();
        WriteGraphToFile(info.footprint, setup, cmd, !useFuture);

        // if (info.encoding.ImpliesFalse()) throw std::logic_error("Failed to perform proper memory update: cautiously refusing to post unsatisfiable encoding."); // TODO better error handling
        CheckPublishing(info);
        CheckReachability(info);
        CheckFlowCoverage(info);
        CheckFlowUniqueness(info);
        CheckInvariant(info);
        AddSpecificationChecks(info);
        AddAffectedOutsideChecks(info);
        AddEffectContextGenerators(info);
        AddEffectPrecisionCheck(info);
        info.encoding.Check();

        MinimizeFootprint(info);
        auto effects = ExtractEffects(info);
        auto post = ExtractPost(std::move(info));

        plankton::ExtendStack(*post, config, ExtensionPolicy::FAST);
        DEBUG(*post << std::endl << std::endl)
        plankton::InlineAndSimplify(*post);
        if (IsUnsatisfiable(*post)) throw std::logic_error("Failed to perform proper memory update: solver inconsistency suspected."); // TODO better error handling
        return PostImage(std::move(post), std::move(effects));

    } catch (std::logic_error& err) {
        if (!fromFuture) throw err;
        DEBUG("/* from future */ " << *fromFuture << std::endl << std::endl)
        return PostImage(std::move(fromFuture));
    }
}

//
// Footprint to file
//

std::deque<std::shared_ptr<Axiom>> MakeContext(const FlowGraph& graph) {
    std::deque<std::shared_ptr<Axiom>> result;

    // get relevant symbols
    std::set<const SymbolDeclaration*> symbols;
    for (const auto& node : graph.nodes) {
        plankton::InsertInto(plankton::CollectUsefulSymbols(*node.ToLogic(EMode::PRE)), symbols);
        plankton::InsertInto(plankton::CollectUsefulSymbols(*node.ToLogic(EMode::POST)), symbols);
    }

    // get implied stack axioms
    auto candidates = plankton::MakeStackCandidates(*graph.pre->now, ExtensionPolicy::FAST);
    for (const auto& elem : graph.pre->now->conjuncts) {
        if (auto axiom = dynamic_cast<const Axiom*>(elem.get())) candidates.push_back(plankton::Copy(*axiom));
    }
    Encoding encoding(graph);
    for (auto& elem : candidates) {
        if (!dynamic_cast<const StackAxiom*>(elem.get())) continue;
        if (plankton::EmptyIntersection(symbols, plankton::Collect<SymbolDeclaration>(*elem))) continue;
        encoding.AddCheck(encoding.Encode(*elem), [&](bool holds){
            if (holds) result.push_back(std::move(elem));
        });
    }
    encoding.Check();

    // add separation
    for (const auto& node : graph.nodes) {
        for (const auto& other : graph.nodes) {
            if (&node == &other) continue;
            result.push_back(std::make_unique<StackAxiom>(
                    BinaryOperator::NEQ,
                    std::make_unique<SymbolicVariable>(node.address),
                    std::make_unique<SymbolicVariable>(other.address)
            ));
        }
    }

    return result;

    // // get implied stack axioms
    // Encoding encoding(graph);
    // auto annotation = std::make_unique<Annotation>(plankton::Copy(*graph.pre->now));
    // plankton::ExtendStack(*annotation, encoding, ExtensionPolicy::FAST);
    //
    // // add separation
    // for (const auto& node : graph.nodes) {
    //     for (const auto& other : graph.nodes) {
    //         if (&node == &other) continue;
    //         annotation->Conjoin(std::make_unique<StackAxiom>(
    //                 BinaryOperator::NEQ,
    //                 std::make_unique<SymbolicVariable>(node.address),
    //                 std::make_unique<SymbolicVariable>(other.address)
    //         ));
    //     }
    // }
    //
    // // extract stack axioms
    // std::deque<std::shared_ptr<Axiom>> result;
    // for (auto& elem : annotation->now->conjuncts) {
    //     if (auto axiomPtr = dynamic_cast<StackAxiom*>(elem.get())) {
    //         (void) elem.release();
    //         result.push_back(std::unique_ptr<StackAxiom>(axiomPtr));
    //     }
    // }
    // return result;
}

#include <regex>

std::string AsStr(const MemoryWrite& cmd) {
    // return std::regex_replace(plankton::ToString(cmd), std::regex(" "), "");
    return std::regex_replace(plankton::ToString(cmd), std::regex("; "), "");
}

void WriteGraphToFile(const FlowGraph& graph, const std::shared_ptr<EngineSetup>& setup, const MemoryWrite& cmd, bool forFuture) {
    if (!setup || !setup->footprints.is_open()) return;
    std::set<const SymbolDeclaration*> symbols;

    Encoding encoding(graph);
    auto hasEmptyInflow = [&](const auto& node) {
        if (!setup->footprintPrecision || node.parent.GetRoot().address == node.address)
            return encoding.Implies(InflowEmptinessAxiom(node.preAllInflow, true));
        return encoding.Implies(InflowEmptinessAxiom(node.frameInflow, true));
    };

    auto& out = setup->footprints;
    auto rename = [](const auto& obj) -> std::string {
        std::stringstream stream;
        stream << obj;
        auto tmp = std::regex_replace(stream.str(), std::regex("@"), "");
        tmp = std::regex_replace(tmp, std::regex("-∞"), "MIN");
        return std::regex_replace(tmp, std::regex("∞"), "MAX");
    };
    auto printField = [&](const auto& field) {
        symbols.insert(&field.preValue.get());
        symbols.insert(&field.postValue.get());
        out << "        @field " << rename(field.name) << " : " << field.type.name;
        if (field.type.sort == Sort::PTR) out << "*";
        out << " = " << rename(field.preValue.get()) << " / " << rename(field.postValue.get()) << ";" << std::endl;
    };

    out << std::endl << std::endl;
    out << "@graph { " << std::endl;
    out << "    #name \"" << (forFuture ? "FUT" : "CMD") << "(" << AsStr(cmd) << ")\"" << std::endl;
    for (const auto& node : graph.nodes) {
        symbols.insert(&node.address);
        out << "    @node[" << rename(node.address) << " : " << node.address.type.name << "*] {" << std::endl;
        if (node.preLocal) out << "        @pre-unreachable;" << std::endl;
        if (hasEmptyInflow(node)) out << "        @pre-emptyInflow;" << std::endl;
        for (const auto& field : node.dataFields) printField(field);
        for (const auto& field : node.pointerFields) printField(field);
        out << "    }" << std::endl;
    }
    for (const auto& axiom : MakeContext(graph)) {
        auto axiomSymbols = plankton::Collect<SymbolDeclaration>(*axiom);
        if (!plankton::Subset(axiomSymbols, symbols)) continue;
        out << "    @constraint " << rename(*axiom) << ";" << std::endl;
    }
    out << "}" << std::endl;
}
