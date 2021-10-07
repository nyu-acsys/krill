#include "engine/linearizability.hpp"

#include "engine/proof.hpp"

using namespace plankton;


bool plankton::IsLinearizable(const Program& program, const SolverConfig& config) {
    ProofGenerator proof(program, config);
    proof.GenerateProof();
    return true;
}
