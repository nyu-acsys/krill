#pragma once
#ifndef COLA_SIMPLIFY
#define COLA_SIMPLIFY


#include <memory>
#include <vector>
#include <map>
#include <string>
#include <exception>
#include "cola/ast.hpp"


namespace cola {

	struct TransformationError : public std::exception {
		const std::string cause;
		TransformationError(std::string cause_) : cause(cause_) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
	};

	/** Removes conditions from do-while and while loops.
	  */
	void remove_conditional_loops(Program& program);

	/** Flattens scopes of scopes into scopes.
	  */
	void remove_useless_scopes(Program& program);

	/** Replaces CAS with atomic blocks.
	  */
	void remove_cas(Program& program);

	/** Rewrites program to CoLa Light.
	  */
	inline void simplify(Program& program) {
		// TODO: loop peeling?
		remove_conditional_loops(program);
		remove_cas(program);
		remove_useless_scopes(program);
	}

} // namespace cola

#endif