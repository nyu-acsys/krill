#pragma once
#ifndef PLANKTON_LOGGER
#define PLANKTON_LOGGER


#include <iostream>
#include "cola/ast.hpp"
#include "cola/util.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"


namespace plankton {

	struct Logger {
		template<typename T, typename = std::enable_if_t<
			!std::is_base_of_v<heal::Formula, T> && !std::is_base_of_v<cola::AstNode, T>
		>>
		Logger& operator<<(const T& val) {
			std::cout << val;
			return *this;
		}

		Logger& operator<<(std::ostream& (*pf)(std::ostream&)) {
			std::cout << pf;
			return *this;
		}

		Logger& operator<<(const cola::Expression& node) {
			cola::print(node, std::cout);
			return *this;
		}

		Logger& operator<<(const cola::Command& node) {
			cola::print(node, std::cout);
			return *this;
		}

		Logger& operator<<(const cola::Statement& node) {
			cola::print(node, std::cout);
			return *this;
		}

		Logger& operator<<(const heal::Formula& formula) {
			heal::print(formula, std::cout);
			return *this;
		}

		operator std::ostream&() const {
			return std::cout;
		}
	};

	inline Logger& log() {
		static Logger logger;
		return logger;
	}

} // namespace plankton

#endif