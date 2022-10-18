#include "engine/linearizability.hpp"

#include "engine/proof.hpp"

using namespace plankton;


bool plankton::IsLinearizable(const Program& program, const SolverConfig& config, std::shared_ptr<EngineSetup> setup) {
    ProofGenerator proof(program, config, std::move(setup));
    proof.GenerateProof();
    return true;
}
