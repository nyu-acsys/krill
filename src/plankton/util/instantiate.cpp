#include "plankton/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


std::unique_ptr<Axiom> plankton::instantiate_property(const Property& property, std::vector<std::reference_wrapper<const VariableDeclaration>> vars) {
	// check compatibility
	if (property.vars.size() != vars.size()) {
		throw std::logic_error("Cannot instantiate property '" + property.name + "', requires " + std::to_string(property.vars.size()) + " arguments but " + std::to_string(vars.size()) + " are given.");
	}
	for (std::size_t index = 0; index < property.vars.size(); ++index) {
		const auto& type = property.vars.at(index)->type;
		const auto& other = vars.at(index).get().type;
		if (&type != &other) {
			throw std::logic_error("Cannot instantiate property '" + property.name + "', requires '" + type.name + "' for " + std::to_string(index) + "-th argument but '" + other.name + "' is given.");
		}
	}

	// instantiate
	std::vector<VariableExpression> dummy_expr(vars.begin(), vars.end());
	std::unique_ptr<Axiom> result = std::make_unique<ExpressionAxiom>(cola::copy(*property.expr));
	return plankton::replace_expression(std::move(result), [&](const auto& expr) {
		std::unique_ptr<Expression> replacement;

		for (const auto& varexp : dummy_expr) {
			if (plankton::syntactically_equal(expr, varexp)) {
				replacement = std::make_unique<VariableExpression>(varexp.decl);
			}
			break;
		}

		return std::make_pair(replacement ? true : false, std::move(replacement));
	});
}