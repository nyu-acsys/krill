#include "engine/util.hpp"

#include "logics/util.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


bool ReachSet::IsReachable(const SymbolDeclaration& source, const SymbolDeclaration& target) const {
    return plankton::Membership(container.at(&source), &target);
}

const std::set<const SymbolDeclaration*>& ReachSet::GetReachable(const SymbolDeclaration& source) const {
    static std::set<const SymbolDeclaration*> empty;
    auto find = container.find(&source);
    if (find != container.end()) return find->second;
    return empty;
}

#include "util/log.hpp"
inline ReachSet ComputeReach(ReachSet initial) {
    ReachSet reachability = std::move(initial);
    bool changed;
    do {
        changed = false;
        for (auto& [node, nodeReach] : reachability.container) {
            assert(node);
            auto size = nodeReach.size();
            for (const auto* value : nodeReach) {
                if (value->type.sort != Sort::PTR) continue;
                const auto& valueReach = reachability.container[value];
                nodeReach.insert(valueReach.begin(), valueReach.end());
            }
            changed |= size != nodeReach.size();
        }
    } while (changed);
    return reachability;
}

ReachSet plankton::ComputeReachability(const Formula& formula) {
    ReachSet initial;
    for (const auto* memory : plankton::Collect<MemoryAxiom>(formula)) {
        for (const auto& pair : memory->fieldToValue) {
            if (pair.second->GetSort() != Sort::PTR) continue;
            initial.container[&memory->node->Decl()].insert(&pair.second->Decl());
        }
    }
    return ComputeReach(std::move(initial));
}

ReachSet plankton::ComputeReachability(const FlowGraph& graph, EMode mode) {
    ReachSet initial;
    for (const auto& node : graph.nodes) {
        for (const auto& field : node.pointerFields) {
            assert(field.Value(mode).type.sort == Sort::PTR);
            initial.container[&node.address].insert(&field.Value(mode));
        }
    }
    return ComputeReach(std::move(initial));
}
