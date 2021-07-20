#include "cola/parse.hpp"

#include <sstream>
#include <iostream>
#include <fstream>
#include "antlr4-runtime.h"
#include "CoLaLexer.h"
#include "CoLaParser.h"
#include "cola/ast.hpp"
#include "AstBuilder.hpp"

using namespace antlr4;
using namespace cola;


std::string get_path(std::string filename) {
	// #include <filesystem>
	// return std::filesystem::path(filename).parent_path();

	auto find = filename.rfind('/');
	if (find != std::string::npos) {
		return filename.substr(0, find+1);
	} else {
		return "";
	}
}


std::shared_ptr<Program> cola::parse_program(std::string filename) {
	std::ifstream file(filename);
	auto result = parse_program(file);
	result->options["_path"] = get_path(filename);
	// std::cout << get_path(filename) << std::endl;
	return result;
}

std::shared_ptr<Program> cola::parse_program(std::istream& input) {
	ANTLRInputStream antlr(input);
	CoLaLexer lexer(&antlr);
	CommonTokenStream tokens(&lexer);

	CoLaParser parser(&tokens);
	auto programContext = parser.program();

	if (parser.getNumberOfSyntaxErrors() != 0) {
		// TODO: proper error class
		throw ParseError();
	}

	return AstBuilder::buildFrom(programContext);
}
