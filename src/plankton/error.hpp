#pragma once
#ifndef PLANKTON_ERROR
#define PLANKTON_ERROR


#include <string>
#include <ostream>
#include "cola/ast.hpp"


namespace plankton {

	struct PlanktonError : public std::exception {
		const std::string cause;
		PlanktonError(std::string cause) : cause(std::move(cause)) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
	};

	struct VerificationError : public PlanktonError {
		VerificationError(std::string cause);
	};

	struct UnsupportedConstructError : public VerificationError {
		UnsupportedConstructError(std::string construct);
	};

	struct AssertionError : public VerificationError {
		AssertionError(const cola::Assert& cmd);
	};

	struct UnsafeDereferenceError : public VerificationError {
		UnsafeDereferenceError(const cola::Expression& expr);
		UnsafeDereferenceError(const cola::Expression& expr, const cola::Command& where);
	};

	struct SolvingError : public VerificationError {
		SolvingError(std::string cause);
	};

	struct EncodingError : public VerificationError {
		EncodingError(std::string cause);
	};

	struct InvariantViolationError : public VerificationError {
		InvariantViolationError(std::string cause);
	};

} // namespace plankton

#endif