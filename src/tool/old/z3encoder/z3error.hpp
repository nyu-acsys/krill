#pragma once
#ifndef SOLVER_ENCODER_Z3ERROR
#define SOLVER_ENCODER_Z3ERROR


#include "prover/error.hpp" // TODO: refactor
#include "solver/encoding.hpp"


namespace solver {

struct Z3Error : public prover::PlanktonError {
		Z3Error(std::string cause);
	};

	struct Z3EncodingError : public Z3Error {
		Z3EncodingError(std::string cause);
	};

	struct Z3DowncastError : public Z3EncodingError {
		Z3DowncastError(const EncodedTerm& term);
		Z3DowncastError(const EncodedSymbol& symbol);
	};

	struct Z3SolvingError : public Z3Error {
		Z3SolvingError(std::string cause);
	};

} // namespace engine

#endif