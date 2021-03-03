#pragma once
#ifndef PROVER_BACKEND_Z3ERROR
#define PROVER_BACKEND_Z3ERROR


#include "prover/error.hpp"
#include "prover/encoding.hpp"


namespace prover {

	struct Z3Error : public PlanktonError {
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

} // namespace prover

#endif