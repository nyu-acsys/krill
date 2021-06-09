#pragma once
#ifndef SOLVER_EXPAND
#define SOLVER_EXPAND

#include "cola/ast.hpp"
#include "heal/logic.hpp"

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

    static inline bool IsGuaranteedNotEqual(const heal::SymbolicVariableDeclaration& address, const heal::SymbolicVariableDeclaration& compare, const heal::Formula& state) {
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

        // add invariants
        for (const auto* memory : heal::CollectMemory(*state)) {
            state->conjuncts.push_back(config.GetNodeInvariant(*memory));
        }
        for (const auto* var : heal::CollectVariables(*state)) {
            if (!var->variable->decl.is_shared) continue;
            state->conjuncts.push_back(config.GetSharedVariableInvariant(*var));
        }

        // prepare for solving
        z3::context context;
        z3::solver solver(context);
        Z3Encoder encoder(context, solver);
        ImplicationCheckSet checks(context);
        solver.add(encoder(*state));

        // check for NULL and non-NULL
        std::map<const heal::SymbolicVariableDeclaration*, std::optional<bool>> isNonNull;
        auto updateNonNull = [&isNonNull,&state](auto* address, bool value){
            isNonNull[address] = value;
            auto op = value ? heal::SymbolicAxiom::NEQ : heal::SymbolicAxiom::EQ;
            state->conjuncts.push_back(std::make_unique<heal::SymbolicAxiom>(std::make_unique<heal::SymbolicVariable>(*address), op,
                                                                             std::make_unique<heal::SymbolicNull>()));
        };
        for (const auto* address : addresses) {
            auto addressIsNull = encoder.MakeNullCheck(*address);
            checks.Add(addressIsNull, [&updateNonNull,address](bool holds) { if (holds) updateNonNull(address, false); });
            checks.Add(!addressIsNull, [&updateNonNull,address](bool holds) { if (holds) updateNonNull(address, true); });
        }
        solver::ComputeImpliedCallback(solver, checks);
        heal::InlineAndSimplify(*state);

        // find new points-to predicates
        for (const auto* address : addresses) {
            if (!isNonNull[address].value_or(false)) continue;

            // find addresses that are guaranteed to be different from 'address'
            Reachability reachability(*state); // TODO: lazily rebuild only when state has changed
            auto potentiallyOverlapping = heal::CollectMemory(*state); // TODO: lazily rebuild only when state has changed
            bool potentiallyOverlappingWithLocal = false;
            for (auto it = potentiallyOverlapping.begin(); !potentiallyOverlappingWithLocal && it != potentiallyOverlapping.end();) {
                bool isDistinct = reachability.IsReachable((*it)->node->Decl(), *address)
                                  || IsGuaranteedNotEqual(*address, (*it)->node->Decl(), *state);
                if (isDistinct) it = potentiallyOverlapping.erase(it);
                else {
                    potentiallyOverlappingWithLocal |= (*it)->isLocal;
                    ++it;
                }
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
            state->conjuncts.push_back(config.GetNodeInvariant(*newPointsTo));
            state->conjuncts.push_back(std::move(newPointsTo));
            heal::InlineAndSimplify(*state);
        }

        return state;
    }

}

#endif
