#pragma once
#ifndef PLANKTON_ENGINE_STATIC_HPP
#define PLANKTON_ENGINE_STATIC_HPP

#include <set>
#include <deque>
#include "programs/ast.hpp"
#include "logics/ast.hpp"

namespace plankton {
    
    struct DataFlowAnalysis {
        explicit DataFlowAnalysis(const Program& program);
        
        [[nodiscard]] bool AlwaysPointsToShared(const VariableDeclaration& decl) const;
        
        private:
            std::set<const VariableDeclaration*> alwaysShared;
    };


    struct FutureSuggestion {
        std::unique_ptr<Guard> guard;
        std::unique_ptr<MemoryWrite> command;

        explicit FutureSuggestion(std::unique_ptr<MemoryWrite> command);
        explicit FutureSuggestion(std::unique_ptr<MemoryWrite> command, std::unique_ptr<Guard> guard);
    };

    std::ostream& operator<<(std::ostream& stream, const FutureSuggestion& object);

    [[nodiscard]] std::deque<std::unique_ptr<FutureSuggestion>> SuggestFutures(const Program& program);
    
}

#endif //PLANKTON_ENGINE_STATIC_HPP
