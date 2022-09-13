#include "engine/linearizability.hpp"

#include "engine/proof.hpp"

using namespace plankton;


bool plankton::IsLinearizable(const Program& program, const SolverConfig& config, EngineSetup setup) {
    ProofGenerator proof(program, config, setup);
    proof.GenerateProof();
    return true;
}
