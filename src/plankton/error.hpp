#pragma once
#ifndef PLANKTON_ERROR
#define PLANKTON_ERROR


#include <string>
#include <ostream>
#include "cola/ast.hpp"


namespace plankton {

	struct VerificationError : public std::exception {
		const std::string cause;
		VerificationError(std::string cause_) : cause(std::move(cause_)) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
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
		SolvingError(std::string cause_);
	};

	struct EncodingError : public VerificationError {
		EncodingError(std::string cause_);
	};

} // namespace plankton

#endif