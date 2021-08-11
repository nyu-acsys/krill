#pragma once
#ifndef PLANKTON_UTIL_LOG_HPP
#define PLANKTON_UTIL_LOG_HPP

#include <iostream>

namespace plankton {
    
    #define DEBUG(X) { std::cout << X; }
    
    #define INFO(X) { std::cout << X; }
    
    #define WARNING(X) { std::cerr << "WARNING: " << X; }
    

//	struct Logger {
//
//		Logger& operator<<(std::ostream& (*pf)(std::ostream&)) {
//			std::cout << pf;
//			return *this;
//		}
//
//		Logger& operator<<(const cola::Expression& node) {
//			cola::print(node, std::cout);
//			return *this;
//		}
//
//		Logger& operator<<(const cola::Command& node) {
//			cola::print(node, std::cout);
//			return *this;
//		}
//
//		Logger& operator<<(const cola::Statement& node) {
//			cola::print(node, std::cout);
//			return *this;
//		}
//
//		Logger& operator<<(const heal::Formula& formula) {
//			heal::Print(formula, std::cout);
//			return *this;
//		}
//
//		operator std::ostream&() const {
//			return std::cout;
//		}
//	};
//
//	inline Logger& log() {
//		static Logger logger;
//		return logger;
//	}

} // namespace plankton

#endif //PLANKTON_UTIL_LOG_HPP