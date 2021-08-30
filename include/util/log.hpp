#pragma once
#ifndef PLANKTON_UTIL_LOG_HPP
#define PLANKTON_UTIL_LOG_HPP

#include <iostream>

namespace plankton {
    
    // void LogMsg(const std::string& msg, std::size_t level = 0) { /* ... */ }
    // #define LOG(...) LogMsg(__VA_ARGS__)

    #ifdef ENABLE_DEBUG_PRINTING
        #define DEBUG(X) { std::cout << X; }
    #else
        #define DEBUG(X) {}
    #endif
    
    #define INFO(X) { std::cout << X; }

    #define WARNING(X) { std::cerr << "WARNING: " << X; }
    
    #define ERROR(X) { std::cerr << "ERROR: " << X; }

} // namespace plankton

#endif //PLANKTON_UTIL_LOG_HPP