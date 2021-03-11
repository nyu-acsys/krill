#include "../../include/prover/error.hpp"

#include <sstream>
#include "../../include/cola/util.hpp"

using namespace cola;
using namespace prover;


VerificationError::VerificationError(std::string cause_) : PlanktonError(std::move(cause_)) {}

UnsupportedConstructError::UnsupportedConstructError(std::string construct) : VerificationError(
	"Unsupported construct: " + std::move(construct) + ".") {}

inline std::string mk_assert_msg(const Assert& cmd) {
	std::stringstream msg;
	msg << "Assertion error: ";
	cola::print(cmd, msg);
	return msg.str();
}

AssertionError::AssertionError(const Assert& cmd) : VerificationError(mk_assert_msg(cmd)) {}

inline std::string mk_deref_msg(const Expression& expr, const Command* cmd=nullptr) {
	std::stringstream msg;
	msg << "Unsafe dereference: '";
	cola::print(expr, msg);
	msg << "' might be 'NULL'";
	if (cmd) {
		msg << " in command '";
		cola::print(*cmd, msg);
		msg << "'";
	}
	msg << ".";
	return msg.str();
}

UnsafeDereferenceError::UnsafeDereferenceError(const Expression& expr) : VerificationError(mk_deref_msg(expr)) {}

UnsafeDereferenceError::UnsafeDereferenceError(const Expression& expr, const Command& cmd) : VerificationError(mk_deref_msg(expr, &cmd)) {}

SolvingError::SolvingError(std::string cause) : VerificationError(std::move(cause)) {}

EncodingError::EncodingError(std::string cause) : VerificationError(std::move(cause)) {}

InvariantViolationError::InvariantViolationError(std::string cause)  : VerificationError(std::move(cause)) {}

inline std::string to_th(std::size_t index) {
	switch (index) {
		case 1: return "1st";
		case 2: return "2nd";
		default: return std::to_string(index) + "th";
	}
}
