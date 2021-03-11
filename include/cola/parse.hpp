#pragma once
#ifndef COLA_PARSE
#define COLA_PARSE

#include <istream>
#include <string>
#include <memory>
#include <exception>
#include "ast.hpp"


namespace cola {

	struct ParseError : public std::exception {
		const std::string cause;
		ParseError(std::string cause_="unknown (sorry)") : cause(std::move(cause_)) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
	};

	std::shared_ptr<Program> parse_program(std::string filename);

	std::shared_ptr<Program> parse_program(std::istream& input);

} // namespace cola

#endif