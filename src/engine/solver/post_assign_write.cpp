#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/flowgraph.hpp"
#include "engine/util.hpp"
#include "util/log.hpp"

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
    std::deque<std::unique_ptr<Axiom>> postSpecifications;
    std::optional<bool> isPureUpdate = std::nullopt;
    
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> outsideInsideAlias;
    std::map<const SymbolDeclaration*, std::set<const SymbolDeclaration*>> outsideInsideDistinct;
    std::map<const SymbolDeclaration*, std::deque<std::unique_ptr<Axiom>>> effectContext;
    
    explicit PostImageInfo(std::unique_ptr<Annotation> pre_, const MemoryWrite& cmd, const SolverConfig& config)
            : config(config), command(cmd), footprint(plankton::MakeFlowFootprint(std::move(pre_), cmd, config)),
              encoding(footprint), pre(*footprint.pre), preObligations(plankton::Collect<ObligationAxiom>(*pre.now)) {
        DEBUG("** pre after footprint creation: " << *footprint.pre << std::endl)
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

inline void CheckPublishing(PostImageInfo& info) {
    for (auto& node : info.footprint.nodes) {
        if (node.preLocal == node.postLocal) continue;
        node.needed = true;
        for (auto& field : node.pointerFields) {
            auto next = info.footprint.GetNodeOrNull(field.postValue);
            if (!next) throw std::logic_error("Footprint too small to capture publishing"); // TODO: better error handling
            next->needed = true;
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
            if (node.preLocal)
                if (auto reachNode = info.footprint.GetNodeOrNull(*reach))
                    if (!reachNode->preLocal) continue;
            
            if (plankton::Subset(preReach.GetReachable(*reach), preReach.GetReachable(node.address)))
                continue;

            info.encoding.AddCheck(info.encoding.EncodeIsNull(*reach), [reach](bool holds){
                if (holds) return;
                throw std::logic_error("Update failed: cannot guarantee acyclicity via potentially non-null non-footprint address " + reach->name + "."); // TODO: better error handling
            });
        }
    }
}

inline void AddFlowCoverageChecks(PostImageInfo& info) {
    auto ensureContains = [&info](const SymbolDeclaration& address){
        // TODO: if address == NULL, then ignore
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
            info.encoding.AddCheck(sameFlow || isPreNull, [ensureContains,&node,&field](bool unchanged){
                DEBUG("outflow of " << node.address.name << " at " << field.name << " is preUnchangedOrNull=" << unchanged << std::endl)
                if (!unchanged) ensureContains(field.preValue);
            });
            info.encoding.AddCheck(sameFlow || isPostNull, [ensureContains,&node,&field](bool unchanged){
                DEBUG("outflow of " << node.address.name << " at " << field.name << " is postUnchangedOrNull=" << unchanged << std::endl)
                if (!unchanged) ensureContains(field.postValue);
            });
        }
    }
}

inline void AddFlowUniquenessChecks(PostImageInfo& info) {
    auto disjointness = info.encoding.EncodeKeysetDisjointness(info.footprint, EMode::POST);
    info.encoding.AddCheck(disjointness, [](bool holds){
        DEBUG("Checking keyset disjointness: holds=" << holds << std::endl)
        if (holds) return;
        throw std::logic_error("Unsafe update: keyset disjointness not guaranteed."); // TODO: better error handling
    });

    auto uniqueness = info.encoding.EncodeInflowUniqueness(info.footprint, EMode::POST);
    info.encoding.AddCheck(uniqueness, [](bool holds){
        DEBUG("Checking inflow uniqueness: holds=" << holds << std::endl)
        if (holds) return;
        throw std::logic_error("Unsafe update: inflow uniqueness not guaranteed."); // TODO: better error handling
    });
}

inline void AddSpecificationChecks(PostImageInfo& info) {
    // check purity
    plankton::AddPureCheck(info.footprint, info.encoding, [&info](bool isPure) {
        DEBUG(" ** is pure=" << isPure << std::endl)
        info.isPureUpdate = isPure;
        if (isPure) {
            for (const auto* obl : info.preObligations) info.postSpecifications.push_back(plankton::Copy(*obl));
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

inline void AddInvariantChecks(PostImageInfo& info) {
    for (const auto& node : info.footprint.nodes) {
        auto nodeInvariant = info.encoding.EncodeNodeInvariant(node, EMode::POST);
        info.encoding.AddCheck(nodeInvariant, [&node](bool holds){
            DEBUG("Checking invariant for " << node.address.name << ": holds=" << holds << std::endl)
            if (holds) return;
            throw std::logic_error("Unsafe update: invariant is not maintained."); // TODO: better error handling
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
// Perform Checks
//

inline void PerformChecks(PostImageInfo& info) {
    info.encoding.Check();
}

inline void MinimizeFootprint(PostImageInfo& info) {
    // plankton::RemoveIf(info.footprint.nodes, [](const auto& node){ return !node.needed; }); // does not work
    std::deque<FlowGraphNode> nodes;
    for (auto& node : info.footprint.nodes) {
        if (!node.needed) continue;
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
        result->flow->decl = factory.GetFresh(result->flow->Type(), result->flow->Order());
    };
    auto updateField = [&result,&factory](const Field& field){
        auto& value = result->fieldToValue.at(field.name);
        value->decl = factory.GetFresh(value->Type(), value->Order());
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
                case Sort::PTR: return { // NULL or not
                    [&symbol]() { return MakeBinary<BinaryOperator::EQ>(symbol, std::make_unique<SymbolicNull>()); },
                    [&symbol]() { return MakeBinary<BinaryOperator::NEQ>(symbol, std::make_unique<SymbolicNull>()); }
                };
            }
        case Order::SECOND:
            return { // empty or not
                [&symbol]() { return std::make_unique<InflowEmptinessAxiom>(symbol, true); },
                [&symbol]() { return std::make_unique<InflowEmptinessAxiom>(symbol, false); }
            };
    }
}

inline void AddEffectContextGenerators(PostImageInfo& info) {
    // collect variables appearing in footprint
    std::set<const SymbolDeclaration*> symbols;
    for (const auto& node : info.footprint.nodes) {
        auto dummy = node.ToLogic(EMode::PRE); // TODO: avoid construction
        plankton::InsertInto(plankton::Collect<SymbolDeclaration>(*dummy), symbols);
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

inline std::unique_ptr<Formula> GetNodeUpdateContext(PostImageInfo& info, const MemoryAxiom& node) {
    auto result = std::make_unique<SeparatingConjunction>();
    auto handle = [&result,&info](const SymbolDeclaration& decl) {
        for (const auto& elem : info.effectContext[&decl]) result->Conjoin(plankton::Copy(*elem));
    };

    handle(node.flow->Decl());
    for (const auto& pair : node.fieldToValue) handle(pair.second->Decl());
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
    auto context = GetNodeUpdateContext(info, *pre);
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
    }
    return result;
}


//
// Overall Algorithm
//

PostImage Solver::Post(std::unique_ptr<Annotation> pre, const MemoryWrite& cmd) const {
    DEBUG("POST for " << *pre << " " << cmd << std::endl)

    // TODO: use futures
    PrepareAccess(*pre, cmd);
    plankton::InlineAndSimplify(*pre);
    PostImageInfo info(std::move(pre), cmd, config);

    CheckPublishing(info);
    CheckReachability(info);
    AddFlowCoverageChecks(info);
    AddFlowUniquenessChecks(info);
    AddSpecificationChecks(info);
    AddInvariantChecks(info);
    AddAffectedOutsideChecks(info);
    AddEffectContextGenerators(info);
    // TODO: extend post stack?
    PerformChecks(info);

    MinimizeFootprint(info);
    auto effects = ExtractEffects(info);
    auto post = ExtractPost(std::move(info));
    
    return PostImage(std::move(post), std::move(effects));
}
