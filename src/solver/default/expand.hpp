#pragma once
#ifndef SOLVER_EXPAND
#define SOLVER_EXPAND

#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "flowgraph.hpp"
#include "post_helper.hpp"

namespace engine {

    struct Reachability {
    private:
        std::map<const heal::SymbolicVariableDeclaration*, std::set<const heal::SymbolicVariableDeclaration*>> reachability;

        void Initialize(std::set<const heal::SymbolicVariableDeclaration*>&& nodes,
                        std::function<std::set<const heal::SymbolicVariableDeclaration*>(const heal::SymbolicVariableDeclaration&)>&& getNext) {
            // populate 1-step successors
            while (!nodes.empty()) {
                auto& decl = **nodes.begin();
                nodes.erase(nodes.begin());
                if (reachability.count(&decl) != 0) continue;
                reachability[&decl] = getNext(decl);
            }

            // populate (n+1)-step successors
            bool changed;
            do {
                changed = false;
                for (auto&[address, reach] : reachability) {
                    auto& addressReach = reachability[address];
                    auto oldSize = addressReach.size();
                    for (auto* next : reach) {
                        if (address == next) continue;
                        auto& nextReach = reachability[next];
                        addressReach.insert(nextReach.begin(), nextReach.end());
                    }
                    changed |= oldSize != addressReach.size();
                }
            } while (changed);
        }

    public:
        explicit Reachability(const heal::Formula& state) {
            LazyValuationMap eval(state);
            Initialize(heal::CollectMemoryNodes(state), [&eval](const auto& decl) {
                std::set<const heal::SymbolicVariableDeclaration*> successors;
                auto resource = eval.GetMemoryResourceOrNull(decl);
                if (resource) {
                    for (const auto&[name, value] : resource->fieldToValue) {
                        if (value->Sort() != cola::Sort::PTR) continue;
                        successors.insert(&value->Decl());
                    }
                }
                return successors;
            });
        }

        explicit Reachability(const FlowGraph& graph, EMode mode) {
            std::set<const heal::SymbolicVariableDeclaration*> nodes;
            for (const auto& node : graph.nodes) nodes.insert(&node.address);
            Initialize(std::move(nodes), [&graph, &mode](const auto& decl) {
                std::set<const heal::SymbolicVariableDeclaration*> successors;
                auto node = graph.GetNodeOrNull(decl);
                if (node) {
                    for (const auto& field : node->pointerFields) {
                        successors.insert(mode == EMode::PRE ? &field.preValue.get() : &field.postValue.get());
                    }
                }
                return successors;
            });
        }

        bool IsReachable(const heal::SymbolicVariableDeclaration& src, const heal::SymbolicVariableDeclaration& dst) {
            return reachability[&src].count(&dst) != 0;
        }

        const std::set<const heal::SymbolicVariableDeclaration*>& GetReachable(const heal::SymbolicVariableDeclaration& src) {
            return reachability[&src];
        }
    };

    template<typename C>
    static inline std::set<const heal::SymbolicVariableDeclaration*, C> SortSet(const std::set<const heal::SymbolicVariableDeclaration*>& set, const C& comparator) {
        std::set<const heal::SymbolicVariableDeclaration*, C> result(comparator);
        result.insert(set.begin(), set.end());
        return result;
    }

    struct PointsToRemover : heal::DefaultLogicNonConstListener {
        const std::set<const heal::PointsToAxiom*>& prune;
        explicit PointsToRemover(const std::set<const heal::PointsToAxiom*>& prune) : prune(prune) {}
        void visit(heal::SeparatingImplication& obj) override { obj.conclusion->accept(*this); } // do not weaken premise
        inline void Handle(decltype(heal::SeparatingConjunction::conjuncts)& conjuncts) {
            conjuncts.erase(std::remove_if(conjuncts.begin(), conjuncts.end(), [this](const auto& elem){
                return prune.count(dynamic_cast<const heal::PointsToAxiom*>(elem.get())) != 0;
            }), conjuncts.end());
        }
        void enter(heal::SeparatingConjunction& obj) override { Handle(obj.conjuncts); }
    };

    static inline std::unique_ptr<heal::PointsToAxiom> MakePointsTo(const heal::SymbolicVariableDeclaration& address, heal::SymbolicFactory& factory, const SolverConfig& config) {
        bool isLocal = false; // TODO: is that correct?
        auto& flow = factory.GetUnusedFlowVariable(config.GetFlowValueType());
        std::map<std::string, std::unique_ptr<heal::SymbolicVariable>> fieldToValue;
        for (const auto& [name, type] : address.type.fields) {
            fieldToValue.emplace(name, std::make_unique<heal::SymbolicVariable>(factory.GetUnusedSymbolicVariable(type)));
        }
        return std::make_unique<heal::PointsToAxiom>(std::make_unique<heal::SymbolicVariable>(address), isLocal, flow, std::move(fieldToValue));
    }

    static inline bool IsSyntacticallyDistinct(const heal::SymbolicVariableDeclaration& address, const heal::SymbolicVariableDeclaration& compare, const heal::Formula& state) {
        struct DistinctCheck : public heal::BaseResourceVisitor {
            const heal::SymbolicVariableDeclaration& address;
            const heal::SymbolicVariableDeclaration& compare;
            bool distinct = false;
            explicit DistinctCheck(decltype(address) address, decltype(compare) compare) : address(address), compare(compare) {}
            void enter(const heal::SymbolicAxiom& axiom) override {
                if (distinct) return;
                if (axiom.op != heal::SymbolicAxiom::NEQ) return;
                if (auto lhs = dynamic_cast<const heal::SymbolicVariable*>(axiom.lhs.get())) {
                    if (auto rhs = dynamic_cast<const heal::SymbolicVariable*>(axiom.rhs.get())) {
                        distinct |= (&lhs->Decl() == &address && &rhs->Decl() == &compare)
                                 || (&lhs->Decl() == &compare && &rhs->Decl() == &address);
                    }
                }
            }
        } checker(address, compare);
        state.accept(checker);
        if (checker.distinct) return true;
        auto memory = FindMemory(address, state); // TODO: lazy eval
        auto other = FindMemory(compare, state); // TODO: lazy eval
        if (memory && other) return &memory->node->Decl() != &other->node->Decl();
        return false;
    }

    static std::unique_ptr<heal::SeparatingConjunction>
    ExpandMemoryFrontier(std::unique_ptr<heal::SeparatingConjunction> state, heal::SymbolicFactory& factory, const SolverConfig& config,
                         const std::set<const heal::SymbolicVariableDeclaration*>& expansionAddresses,
                         std::set<const heal::SymbolicVariableDeclaration*>&& forceExpansion = {}, bool forceFail = true) {
        // TODO: move removed overlap into a past predicate

        static Timer timer("engine::ExpandMemoryFrontier");
        auto measurement = timer.Measure();

        if (expansionAddresses.empty()) return state;

        // add force aliases
        for (const auto* force : forceExpansion) {
            if (auto memory = engine::FindMemory(*force, *state)) {
                forceExpansion.insert(&memory->node->Decl());
            }
        }

        // sort expansion
        auto addresses = SortSet(expansionAddresses, [forceExpansion](auto symbol, auto other){
            // forceExpansion is sorted to the front
            auto symbolForced = forceExpansion.count(symbol) != 0;
            auto otherForced = forceExpansion.count(other) != 0;
            if (symbolForced && otherForced) return symbol < other;
            if (!symbolForced && !otherForced) return symbol < other;
            return symbolForced;
        });
        for (auto it = addresses.begin(); it != addresses.end();) {
            if (engine::FindMemory(**it, *state)) it = addresses.erase(it);
            else ++it;
        }
        if (addresses.empty()) return state;

        // prepare for solving
        z3::context context;
        z3::solver solver(context);
        Z3Encoder encoder(context, solver);
        ImplicationCheckSet checks(context);
        solver.add(encoder(*state));

        // add invariants
        auto memoryResources = heal::CollectMemory(*state);
        auto variableResources = heal::CollectVariables(*state);
        for (const auto* memory : memoryResources) {
            solver.add(encoder(*config.GetNodeInvariant(*memory)));
        }
        for (const auto* var : variableResources) {
            if (!var->variable->decl.is_shared) continue;
            solver.add(encoder(*config.GetSharedVariableInvariant(*var)));
        }

        // add locality constraints
        for (const auto* local : memoryResources) {
            if (!local->isLocal) continue;
            auto localNode = encoder(*local->node);

            for (const auto* shared : memoryResources) {
                if (shared->isLocal) continue;
                solver.add(localNode != encoder(*shared->node));
                for (const auto& [field, value] : shared->fieldToValue) {
                    if (value->Sort() != cola::Sort::PTR) continue;
                    solver.add(localNode != encoder(*value));
                }
            }
            for (const auto* shared : variableResources) {
                if (!shared->variable->decl.is_shared) continue;
                solver.add(localNode != encoder(*shared->value));
            }
        }

        // check for NULL and non-NULL, derive inequalities among pointers
        std::map<const heal::SymbolicVariableDeclaration*, std::optional<bool>> isNonNull;
        auto updateNonNull = [&isNonNull,&state](auto* address, bool value){
            isNonNull[address] = value;
            auto op = value ? heal::SymbolicAxiom::NEQ : heal::SymbolicAxiom::EQ;
            state->conjuncts.push_back(std::make_unique<heal::SymbolicAxiom>(std::make_unique<heal::SymbolicVariable>(*address), op,
                                                                             std::make_unique<heal::SymbolicNull>()));
        };
        auto addInequality = [&state](const auto& address, const auto& other){
            state->conjuncts.push_back(std::make_unique<heal::SymbolicAxiom>(std::make_unique<heal::SymbolicVariable>(address), heal::SymbolicAxiom::NEQ,
                                                                             std::make_unique<heal::SymbolicVariable>(other)));
        };
        for (const auto* address : addresses) {
            // null / non-null
            auto addressIsNull = encoder.MakeNullCheck(*address);
            checks.Add(addressIsNull, [&updateNonNull,address](bool holds) { if (holds) updateNonNull(address, false); });
            checks.Add(!addressIsNull, [&updateNonNull,address](bool holds) { if (holds) updateNonNull(address, true); });

            // inequalities
            for (const auto* memory : memoryResources) {
                auto distinct = encoder(*address) != encoder(*memory->node);
                checks.Add(distinct, [&addInequality,address,other=&memory->node->Decl()](bool holds){
                    if (holds) addInequality(*address, *other);
                });
            }
        }
        engine::ComputeImpliedCallback(solver, checks);
        heal::Simplify(*state);

        // find new points-to predicates
        for (const auto* address : addresses) {
            std::cout << "%% expansion for " << address->name << "  non-null=" << isNonNull[address].value_or(false) << std::endl;
            if (!isNonNull[address].value_or(false)) continue;

            bool forceAddress = forceExpansion.count(address) != 0;
            auto potentiallyOverlapping = heal::CollectMemory(*state); // TODO: lazily rebuild only when state has changed
            bool potentiallyOverlappingWithLocalOrForced = false;
            auto updateOverlapFlag = [&potentiallyOverlappingWithLocalOrForced, forceExpansion](const heal::PointsToAxiom& overlap){
                potentiallyOverlappingWithLocalOrForced |= overlap.isLocal || forceExpansion.count(&overlap.node->Decl()) != 0;
            };

            // syntactically discharge overlap
            auto filter = [&](auto&& isDistinct){
                potentiallyOverlappingWithLocalOrForced = false;
                for (auto it = potentiallyOverlapping.begin(); it != potentiallyOverlapping.end();) {
                    if (isDistinct(*address, (*it)->node->Decl())) it = potentiallyOverlapping.erase(it);
                    else updateOverlapFlag(**(it++));
                }
            };
            filter([&state](const auto& adr, const auto& other){ return IsSyntacticallyDistinct(adr, other, *state); });
            if (!potentiallyOverlapping.empty()) {
                Reachability reachability(*state); // TODO: lazily rebuild only when state has changed
                filter([&reachability](const auto& adr, const auto& other){ return reachability.IsReachable(other, adr); });
            }

            // deeper analysis if forced address is potentially overlapping with local address
            if (potentiallyOverlappingWithLocalOrForced && forceAddress) {
                std::cout << ">> local overlap on forced address detected, performing deep analysis..." << std::endl;
                auto graph = engine::MakePureHeapGraph(std::make_unique<heal::Annotation>(heal::Copy(*state)), config); // TODO: avoid copy?
                if (!graph.nodes.empty()) {
                    EncodedFlowGraph encoding(std::move(graph));
                    ImplicationCheckSet localChecks(encoding.context);
                    for (const auto* memory : potentiallyOverlapping) {
                        auto distinct = encoding.encoder(*address) != encoding.encoder(*memory->node);
                        localChecks.Add(distinct, [memory, &potentiallyOverlapping](bool holds) {
                            std::cout << "   --> overlap with " << *memory->node << " pruned=" << holds << std::endl;
                            if (holds) potentiallyOverlapping.erase(memory);
                        });
                    }
                    engine::ComputeImpliedCallback(encoding.solver, localChecks);
                    potentiallyOverlappingWithLocalOrForced = false;
                    for (const auto* memory : potentiallyOverlapping) updateOverlapFlag(*memory);
                }
                if (potentiallyOverlappingWithLocalOrForced && forceFail) {
                    throw std::logic_error("Cannot expand memory frontier on forced address " + address->name + "."); // TODO: better error handling
                }
            }

            // debug
            std::cout << "%% expanding " << address->name << ": forceAddress" << forceAddress << "; localOrForceOverlap=" << potentiallyOverlappingWithLocalOrForced << "; overlapping=";
            for (auto adr : potentiallyOverlapping) std::cout << adr->node->Decl().name << ", ";
            std::cout << std::endl;
            // end debug

            // ignore if overlap and not forced
            if (potentiallyOverlappingWithLocalOrForced) continue;
            if (!potentiallyOverlapping.empty() && !forceAddress) continue;

            // expand
            if (!potentiallyOverlapping.empty()) {
                PointsToRemover remover(potentiallyOverlapping);
                state->accept(remover);
            }
            auto newPointsTo = MakePointsTo(*address, factory, config);
            state->conjuncts.push_back(std::move(newPointsTo));
            heal::Simplify(*state);
        }

        // TODO: derive stack info about new memory?
        return state;
    }

    static std::unique_ptr<heal::SeparatingConjunction>
    ExpandMemoryFrontier(std::unique_ptr<heal::SeparatingConjunction> state, const SolverConfig& config, const heal::Formula* collectFrom= nullptr) {
//        heal::InlineAndSimplify(*state);
        struct Collector : public heal::DefaultLogicListener {
            std::set<const heal::SymbolicVariableDeclaration*> result;
            void enter(const heal::SymbolicVariable& obj) override {
                if (obj.Sort() != cola::Sort::PTR) return;
                result.insert(&obj.Decl());
            }
        } collector;
        if (collectFrom) collectFrom->accept(collector);
        else state->accept(collector);

        heal::SymbolicFactory factory(*state);
        return ExpandMemoryFrontier(std::move(state), factory, config, collector.result);
    }

    static std::unique_ptr<heal::SeparatingConjunction>
    ExpandMemoryFrontierForAccess(std::unique_ptr<heal::SeparatingConjunction> state, const SolverConfig& config, const cola::Expression& accessesFrom) {
        static Timer timer("engine::ExpandMemoryFrontierForAccess");
        auto measurement = timer.Measure();

        auto dereferences = GetDereferences(accessesFrom);
        if (dereferences.empty()) return state;
        std::set<const heal::SymbolicVariableDeclaration*> addresses;
        for (const auto* dereference : dereferences) {
            auto value = EagerValuationMap(*state).Evaluate(*dereference->expr); // TODO: if this fails, the error will be cryptic
            if (auto var = dynamic_cast<const heal::SymbolicVariable*>(value.get())) addresses.insert(&var->Decl());
        }

        heal::SymbolicFactory factory(*state);
        return ExpandMemoryFrontier(std::move(state), factory, config, addresses, std::move(addresses)); // TODO: avoid repeated invocation
    }

}

#endif
