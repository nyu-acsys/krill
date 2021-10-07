#pragma once
#ifndef PLANKTON_ENGINE_STATIC_HPP
#define PLANKTON_ENGINE_STATIC_HPP

#include <set>
#include "programs/ast.hpp"

namespace plankton {
    
    struct DataFlowAnalysis {
        explicit DataFlowAnalysis(const Program& program);
        
        [[nodiscard]] bool AlwaysPointsToShared(const VariableDeclaration& decl) const;
        
        private:
            std::set<const VariableDeclaration*> alwaysShared;
    };
    
}

#endif //PLANKTON_ENGINE_STATIC_HPP
