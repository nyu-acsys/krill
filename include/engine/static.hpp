#pragma once
#ifndef PLANKTON_ENGINE_STATIC_HPP
#define PLANKTON_ENGINE_STATIC_HPP

#include "programs/ast.hpp"

namespace plankton {
    
    struct DataFlowAnalysis {
        explicit DataFlowAnalysis(const Program& program);
        
        bool NeverPointsToLocal(const VariableDeclaration& decl) const;
    };
    
}

#endif //PLANKTON_ENGINE_STATIC_HPP
