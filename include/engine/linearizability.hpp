#pragma once
#ifndef PLANKTON_ENGINE_LINEARIZABILITY_HPP
#define PLANKTON_ENGINE_LINEARIZABILITY_HPP

#include "programs/ast.hpp"
#include "engine/config.hpp"
#include "setup.hpp"

namespace plankton {

    bool IsLinearizable(const Program& program, const SolverConfig& config, std::shared_ptr<EngineSetup> setup);

    inline bool IsLinearizable(const Program& program, const SolverConfig& config) {
        auto setup = std::make_shared<EngineSetup>();
        return IsLinearizable(program, config, setup);
    }

} // namespace engine

#endif //PLANKTON_ENGINE_LINEARIZABILITY_HPP