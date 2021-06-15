#pragma once
#ifndef SOLVER_EXPAND
#define SOLVER_EXPAND

#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "flowgraph.hpp"
#include "post_helper.hpp"

namespace solver {

    struct Reachability {
    private:
        std::map<const heal::SymbolicVariableDeclaration*, std::set<const heal::SymbolicVariableDeclaration*>> reachability;

        void Initialize(std::set<const heal::SymbolicVariableDeclaration*>&& worklist,
                        std::function<std::set<const heal::SymbolicVariableDeclaration*>(const heal::SymbolicVariableDeclaration&)>&& getNext) {
            // populate 1-step successors
            while (!worklist.empty()) {
                auto& decl = **worklist.begin();
                worklist.erase(worklist.begin());
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
            Initialize({&graph.GetRoot().address}, [&graph, &mode](const auto& decl) {
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
    static inline std::set<const heal::SymbolicVariableDeclaration*, C> CollectAddresses(const heal::LogicObject& object, const C& comparator) {
        struct Collector : public heal::DefaultLogicListener {
            std::set<const heal::SymbolicVariableDeclaration*, C> result;
            explicit Collector(const C& comparator) : result(comparator) {}
            void enter(const heal::SymbolicVariable& obj) override {
                if (obj.Sort() != cola::Sort::PTR) return;
                result.insert(&obj.Decl());
            }
        } collector(comparator);
        object.accept(collector);
        return std::move(collector.result);
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
        if (!memory || !other) return false;
        return &memory->node->Decl() != &other->node->Decl();
    }

    static std::unique_ptr<heal::SeparatingConjunction>
    ExpandMemoryFrontier(std::unique_ptr<heal::SeparatingConjunction> state, heal::SymbolicFactory& factory, const SolverConfig& config,
                         const heal::LogicObject& addressesFrom, const heal::SymbolicVariableDeclaration* forceExpansion = nullptr) {

        // find all address that need expansion
        heal::InlineAndSimplify(*state);
        auto addresses = CollectAddresses(addressesFrom, [forceExpansion](auto symbol, auto other){
            // forceExpansion is the first element in the set
            if (symbol == forceExpansion || other == forceExpansion) return symbol == forceExpansion && other != forceExpansion;
            else return symbol < other;
        });
        for (auto it = addresses.begin(); it != addresses.end();) {
            if (solver::FindMemory(**it, *state)) it = addresses.erase(it);
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
        solver::ComputeImpliedCallback(solver, checks);
        heal::InlineAndSimplify(*state);

        // find new points-to predicates
        for (const auto* address : addresses) {
            if (!isNonNull[address].value_or(false)) continue;

            auto potentiallyOverlapping = heal::CollectMemory(*state); // TODO: lazily rebuild only when state has changed
            bool potentiallyOverlappingWithLocal = false;

            // syntactically discharge overlap
            auto filter = [&potentiallyOverlapping,&potentiallyOverlappingWithLocal,address](auto&& isDistinct){
                potentiallyOverlappingWithLocal = false;
                for (auto it = potentiallyOverlapping.begin(); it != potentiallyOverlapping.end();) {
                    if (isDistinct(*address, (*it)->node->Decl())) it = potentiallyOverlapping.erase(it);
                    else {
                        potentiallyOverlappingWithLocal |= (*it)->isLocal;
                        ++it;
                    }
                }
            };
            filter([&state](const auto& adr, const auto& other){ return IsSyntacticallyDistinct(adr, other, *state); });
            if (!potentiallyOverlapping.empty()) {
                Reachability reachability(*state); // TODO: lazily rebuild only when state has changed
                filter([&reachability](const auto& adr, const auto& other){ return reachability.IsReachable(other, adr); });
            }

            // deeper analysis if forced address is potentially overlapping with local address
            if (potentiallyOverlappingWithLocal && address == forceExpansion) {
                auto abort = [&address](){ throw std::logic_error("Cannot expand memory frontier on forced address " + address->name + "."); }; // TODO: better error handling
                std::cout << ">> local overlap on forced address detected, performing deep analysis..." << std::endl;
                auto graph = solver::MakeHeapGraph(std::make_unique<heal::Annotation>(heal::Copy(*state)), config); // TODO: avoid copy?
                if (graph.nodes.empty()) abort();
                EncodedFlowGraph encoding(std::move(graph));
                ImplicationCheckSet localChecks(encoding.context);
                for (const auto* memory : potentiallyOverlapping) {
                    auto distinct = encoding.encoder(*address) != encoding.encoder(*memory->node);
                    localChecks.Add(distinct, [memory,&potentiallyOverlapping](bool holds){
                        std::cout << "   --> overlap with " << *memory->node << " pruned=" << holds << std::endl;
                        if (holds) potentiallyOverlapping.erase(memory);
                    });
                }
                solver::ComputeImpliedCallback(encoding.solver, localChecks);
                potentiallyOverlappingWithLocal = false;
                for (const auto* memory : potentiallyOverlapping) potentiallyOverlappingWithLocal |= memory->isLocal;
                if (potentiallyOverlappingWithLocal) abort();
                throw std::logic_error("breakpoint");
            }

            // debug
            std::cout << "%% expanding " << address->name << ": localOverlap=" << potentiallyOverlappingWithLocal << "; overlapping=";
            for (auto adr : potentiallyOverlapping) std::cout << adr->node->Decl().name << ", ";
            std::cout << std::endl;
            // end debug

            // ignore if overlap and not forced
            if (potentiallyOverlappingWithLocal) continue;
            if (!potentiallyOverlapping.empty() && address != forceExpansion) continue;

            // expand
            if (!potentiallyOverlapping.empty()) {
                PointsToRemover remover(potentiallyOverlapping);
                state->accept(remover);
            }
            auto newPointsTo = MakePointsTo(*address, factory, config);
            // state->conjuncts.push_back(config.GetNodeInvariant(*newPointsTo));
            state->conjuncts.push_back(std::move(newPointsTo));
            // heal::InlineAndSimplify(*state);
        }

        return state;
    }

    static std::unique_ptr<heal::SeparatingConjunction>
    ExpandMemoryFrontier(std::unique_ptr<heal::SeparatingConjunction> state, const SolverConfig& config,
                         const heal::LogicObject& addressesFrom, const heal::SymbolicVariableDeclaration* forceExpansion = nullptr) {
        heal::SymbolicFactory factory(*state);
        return ExpandMemoryFrontier(std::move(state), factory, config, addressesFrom, forceExpansion);
    }

    static std::unique_ptr<heal::SeparatingConjunction>
    ExpandMemoryFrontierForAccess(std::unique_ptr<heal::SeparatingConjunction> state, const SolverConfig& config,
                                  const cola::Expression& accessesFrom) {
        auto dereferences = GetDereferences(accessesFrom);
        for (const auto* dereference : dereferences) {
            auto value = EagerValuationMap(*state).Evaluate(*dereference->expr); // TODO: if this fails, the error will be cryptic
            if (auto var = dynamic_cast<const heal::SymbolicVariable*>(value.get())) {
                state = ExpandMemoryFrontier(std::move(state), config, *var, &var->Decl());
            }
        }
        return state;
    }

}

#endif
