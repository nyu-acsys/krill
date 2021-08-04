#pragma once
#ifndef PLANKTON_ENGINE_LINEARIZABILITY_HPP
#define PLANKTON_ENGINE_LINEARIZABILITY_HPP

#include "programs/ast.hpp"
#include "engine/config.hpp"

namespace plankton {
    
    bool IsLinearizable(const Program& program, const SolverConfig& config);
    
} // namespace engine

#endif //PLANKTON_ENGINE_LINEARIZABILITY_HPP