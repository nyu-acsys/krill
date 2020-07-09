#pragma once
#ifndef PLANKTON_PROPERTIES
#define PLANKTON_PROPERTIES


#include <map>
#include <vector>
#include <memory>
#include <type_traits>
#include "cola/ast.hpp"
#include "plankton/error.hpp"
#include "plankton/logic.hpp"
#include "plankton/util.hpp"


namespace plankton {

	enum struct PropertyArity { ONE, TWO, ONE_OR_MORE, ANY };

	void check_property(std::string name, PropertyArity arity, const std::vector<std::unique_ptr<cola::VariableDeclaration>>& vars, const Formula& blueprint);


	template<PropertyArity arity = PropertyArity::ANY, typename T = ConjunctionFormula, EXTENDS_FORMULA(T)>
	struct Property {
		std::string name;
		std::vector<std::unique_ptr<cola::VariableDeclaration>> vars;
		std::unique_ptr<T> blueprint;

		Property(std::string name_, std::vector<std::unique_ptr<cola::VariableDeclaration>> vars_, std::unique_ptr<T> blueprint_)
			: name(std::move(name_)), vars(std::move(vars_)), blueprint(std::move(blueprint_))
		{
			assert(blueprint);
			check_property(name, arity, vars, *blueprint);
		}

		std::size_t get_arity() const {
			return vars.size();
		}

		std::unique_ptr<T> instantiate(std::vector<std::reference_wrapper<const cola::VariableDeclaration>> decls) const {
			if (vars.size() != decls.size()) {
				throw PropertyInstantiationError(name, vars.size(), decls.size());
			}

			// create lookup map
			std::map<const cola::VariableDeclaration*, const cola::VariableDeclaration*> dummy2real;
			for (std::size_t index = 0; index < vars.size(); ++index) {
				const auto& var_type = vars.at(index)->type;
				const auto& decl_type = decls.at(index).get().type;
				if (!cola::assignable(var_type, decl_type)) {
					throw PropertyInstantiationError(name, index, var_type, decl_type);
				}
				dummy2real[vars.at(index).get()] = &decls.at(index).get();
			}

			// create instantiation
			return plankton::replace_expression(plankton::copy(*blueprint), [&dummy2real](const cola::Expression& expr){
				auto [is_var, var_expr] = plankton::is_of_type<cola::VariableExpression>(expr);
				if (is_var) {
					auto find = dummy2real.find(&var_expr->decl);
					if (find != dummy2real.end()) {
						return std::make_pair(true, std::make_unique<cola::VariableExpression>(*find->second));
					}
				}
				return std::make_pair(false, std::unique_ptr<cola::VariableExpression>(nullptr));
			});
		}

		template<typename... Targs>
		std::unique_ptr<T> instantiate(const Targs&... args) const {
			std::vector<std::reference_wrapper<const cola::VariableDeclaration>> decls { args... };
			return instantiate(std::move(decls));
		}
	};


	using Invariant = Property<PropertyArity::ONE, ConjunctionFormula>;
	using Predicate = Property<PropertyArity::TWO, AxiomConjunctionFormula>;
	// using FootprintPredicate = Property<PropertyArity::ONE_OR_MORE, AxiomConjunctionFormula>;

} // namespace plankton

#endif