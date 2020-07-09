#include "plankton/error.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


VerificationError::VerificationError(std::string cause_) : cause(std::move(cause_)) {}

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

PropertyInstantiationError::PropertyInstantiationError(std::string name, std::size_t index, const cola::Type& expected, const cola::Type& got)
	: VerificationError("Instantiation of property '" + name + "' failed: expected " + to_th(index+1) +
		                " argument to be of type '" + expected.name + "' but got incompatible type '" + got.name + "'." ) {}

PropertyInstantiationError::PropertyInstantiationError(std::string name, std::size_t expected, std::size_t got)
	: VerificationError("Instantiation of property '" + name + "' failed: expected " + std::to_string(expected) + " arguments but got " +
		                std::to_string(got) + ".") {}

PropertyConstructionError::PropertyConstructionError(std::string name, std::size_t expected, std::size_t got, std::string cmp)
	: VerificationError("Construction of property '" + name + "' failed: expected " + cmp + std::to_string(expected) + " arguments but got " +
		                std::to_string(got) + ".") {}
