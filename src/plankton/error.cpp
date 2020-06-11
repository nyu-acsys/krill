#include "plankton/error.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


UnsupportedConstructError::UnsupportedConstructError(std::string construct) : VerificationError(
	"Unsupported construct: " + std::move(construct) + ".") {}

inline std::string mk_assert_msg(const Assert& cmd) {
	std::stringstream msg;
	msg << "Assertion error: ";
	cola::print(cmd, msg);
	return msg.str();
}

AssertionError::AssertionError(const Assert& cmd) : VerificationError(mk_assert_msg(cmd)) {}
