#include "plankton/backend/z3error.hpp"

#include <sstream>

using namespace cola;
using namespace heal;
using namespace plankton;

Z3Error::Z3Error(std::string cause) : PlanktonError(std::move(cause)) {}

Z3EncodingError::Z3EncodingError(std::string cause) : Z3Error(std::move(cause)) {}

template<typename T>
std::string MakeDowncastErrorMessage(const T& obj) {
	std::stringstream msg;
	msg << "Failed to downcast to 'Z3Expr': ";
	msg << obj;
	return msg.str();
}

Z3DowncastError::Z3DowncastError(const EncodedTerm& term) : Z3EncodingError(MakeDowncastErrorMessage(term)) {}

Z3DowncastError::Z3DowncastError(const EncodedSymbol& symbol) : Z3EncodingError(MakeDowncastErrorMessage(symbol)) {}

Z3SolvingError::Z3SolvingError(std::string cause) : Z3Error(std::move(cause)) {}
