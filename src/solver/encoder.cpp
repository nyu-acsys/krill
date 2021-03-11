#include "solver/encoder.hpp"
#include "z3encoder/z3encoder.hpp"

using namespace solver;


std::unique_ptr<Encoder> solver::MakeDefaultEncoder(std::shared_ptr<SolverConfig> config) {
    return std::make_unique<Z3Encoder>(config);
}