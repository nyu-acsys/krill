#include "plankton/util.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


bool plankton::syntactically_equal(const Expression& expression, const Expression& other) {
	// TODO: find a better way to do this
	// NOTE: this relies on two distinct VariableDeclaration objects not having the same name
	std::stringstream expressionStream, otherStream;
	cola::print(expression, expressionStream);
	cola::print(other, otherStream);
	return expressionStream.str() == otherStream.str();
}
