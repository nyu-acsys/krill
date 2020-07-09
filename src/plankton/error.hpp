#pragma once
#ifndef PLANKTON_ERROR
#define PLANKTON_ERROR


#include <string>
#include <ostream>
#include "cola/ast.hpp"


namespace plankton {

	struct VerificationError : public std::exception {
		const std::string cause;
		VerificationError(std::string cause);
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
		SolvingError(std::string cause);
	};

	struct EncodingError : public VerificationError {
		EncodingError(std::string cause);
	};

	struct InvariantViolationError : public VerificationError {
		InvariantViolationError(std::string cause);
	};

	struct PropertyConstructionError : public VerificationError {
		PropertyConstructionError(std::string name, std::size_t expected, std::size_t got, std::string cmp="");
	};

	struct PropertyInstantiationError : public VerificationError {
		PropertyInstantiationError(std::string name, std::size_t expected, std::size_t got);
		PropertyInstantiationError(std::string name, std::size_t index, const cola::Type& expected, const cola::Type& got);
	};

} // namespace plankton

#endif