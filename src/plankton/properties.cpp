#include "plankton/properties.hpp"

using namespace cola;
using namespace plankton;


void plankton::check_property(std::string name, PropertyArity arity, const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars, const Formula& /*blueprint*/) {
	switch (arity) {
		case PropertyArity::ONE:
			if (vars.size() != 1) throw PropertyConstructionError(name, 1, vars.size());
			break;

		case PropertyArity::ONE_OR_MORE:
			if (vars.size() < 1) throw PropertyConstructionError(name, 1, vars.size(), ">=");
			break;

		case PropertyArity::TWO:
			if (vars.size() != 2) throw PropertyConstructionError(name, 2, vars.size());
			break;

		case PropertyArity::ANY:
			break;
	}

	// TODO important: implement
	// TODO: ensure that vars are defined
	// TODO: ensure that the variables contained in 'blueprint' are either shared or contained in 'vars'
}