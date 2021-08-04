#pragma once
#ifndef PLANKTON_PROGRAMS_UTIL_HPP
#define PLANKTON_PROGRAMS_UTIL_HPP

#include <string>
#include <type_traits>
#include "ast.hpp"

namespace plankton {
    
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Expression, T> || std::is_base_of_v<Command, T>>>
    std::unique_ptr<T> Copy(const T& object);
    
    void Print(const AstNode& object, std::ostream& stream);
    
    std::string ToString(const Type& object);
    std::string ToString(const Sort& object);
    std::string ToString(const AstNode& object);
    std::string ToString(const VariableDeclaration& object);
    std::string ToString(const BinaryOperator& object);
    
    bool IsRightMover(const Statement& statement);
    
    inline bool IsVoid(const Function& function) { return function.returnType.empty(); }
    
} // namespace plankton

#endif //PLANKTON_PROGRAMS_UTIL_HPP
