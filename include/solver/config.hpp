#pragma once
#ifndef SOLVER_CONFIG
#define SOLVER_CONFIG


#include <memory>
#include "heal/flow.hpp"
#include "heal/properties.hpp"


namespace solver {

    struct SolverConfig {
        /** Suggests an upper bound for the footprint size beyond which a the post image computation may give up.
          */
        const std::size_t maxFootprintDepth;

        /** The flow domain underlying logic formulas.
          */
        const std::unique_ptr<heal::FlowDomain> flowDomain;

        /** A predicate 'P(node, key)' computing whether or not 'node' logically contains 'key'.
          */
        const std::unique_ptr<heal::Predicate> logicallyContainsKey;

        /** An invariant 'I(node)' that is implicitly universally quantified over all nodes in the heap.
          */
        const std::unique_ptr<heal::Invariant> invariant;


        SolverConfig(std::size_t maxFootprintDepth, std::unique_ptr<heal::FlowDomain> flowDomain,
                     std::unique_ptr<heal::Predicate> containsKey, std::unique_ptr<heal::Invariant> invariant)
                : maxFootprintDepth(maxFootprintDepth), flowDomain(std::move(flowDomain)),
                  logicallyContainsKey(std::move(containsKey)), invariant(std::move(invariant)) {
            assert(this->flowDomain);
            assert(this->logicallyContainsKey);
            assert(this->invariant);
        }
    };

} // namespace solver

#endif