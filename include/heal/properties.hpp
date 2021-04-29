#pragma once
#ifndef HEAL_PROPERTIES
#define HEAL_PROPERTIES


#include <map>
#include <array>
#include <memory>
#include "cola/ast.hpp"
#include "logic.hpp"
#include "util.hpp"


namespace heal {

    template<std::size_t N = 1, typename T = SeparatingConjunction, typename = std::enable_if_t<std::is_base_of_v<Formula, T>>>
    struct Property {
        std::string name;
        std::array<std::unique_ptr<SymbolicVariableDeclaration>, N> vars;
        std::unique_ptr<T> blueprint;

        Property(std::string name_, std::array<std::unique_ptr<SymbolicVariableDeclaration>, N> vars_, std::unique_ptr<T> blueprint_);

        std::unique_ptr<T> instantiate(std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> decls) const;

        template<typename... Targs>
        std::unique_ptr<T> instantiate(const Targs&... args) const {
            std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> decls { args... };
            return instantiate(std::move(decls));
        }
    };

	using Invariant = Property<1, SeparatingConjunction>;
	using Predicate = Property<2, SeparatingConjunction>;

} // namespace heal

#endif