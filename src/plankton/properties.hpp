#pragma once
#ifndef PLANKTON_PROPERTIES
#define PLANKTON_PROPERTIES


#include <map>
#include <array>
#include <memory>
#include <type_traits>
#include "cola/ast.hpp"
#include "plankton/util.hpp"
#include "plankton/logic.hpp"


namespace plankton {

	struct PropertyInstantiationError : public std::exception {
		const std::string cause;
		std::string to_th(std::size_t index) {
			switch (index) {
				case 1: return "1st";
				case 2: return "2nd";
				default: return std::to_string(index) + "th";
			}
		}
		PropertyInstantiationError(std::string name, std::size_t index, const cola::Type& expected, const cola::Type& got) : cause(
			"Instantiation of property '" + name + "' failed: expected " + to_th(index+1) + " argument to be of type '" + expected.name + "' but got incompatible type '" + got.name + "'." 
		) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
	};


	template<std::size_t N, typename T = ConjunctionFormula, EXTENDS_FORMULA(T)>
	struct Property {
		std::string name;
		std::array<std::unique_ptr<cola::VariableDeclaration>, N> vars;
		std::unique_ptr<T> blueprint;

		Property(std::string name_, std::array<std::unique_ptr<cola::VariableDeclaration>, N> vars_, std::unique_ptr<T> blueprint_)
			: name(std::move(name_)), vars(std::move(vars_)), blueprint(std::move(blueprint_))
		{
			assert(blueprint);
			// TODO: ensure that the variables contained in 'blueprint' are either shared or contained in 'vars'
		}

		std::unique_ptr<T> instantiate(std::array<std::reference_wrapper<const cola::VariableDeclaration>, N> decls) const {
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
			std::array<std::reference_wrapper<const cola::VariableDeclaration>, N> decls { args... };
			return instantiate(std::move(decls));
		}


	};


	using Invariant = Property<1, ConjunctionFormula>;
	using Predicate = Property<2, AxiomConjunctionFormula>;

} // namespace plankton

#endif