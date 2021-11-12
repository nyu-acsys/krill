#pragma once
#ifndef PLANKTON_UTIL_LOG_HPP
#define PLANKTON_UTIL_LOG_HPP

#include <array>
#include <vector>
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

    struct StatusStack {
        inline void Push(std::string string) { stack.push_back(std::move(string)); }
        inline void Push(std::string string, std::string other) { stack.push_back(std::move(string) + std::move(other)); }
        inline void Push(std::string string, std::size_t other) { stack.push_back(std::move(string) + std::to_string(other)); }
        inline void Pop() { stack.pop_back(); }
        inline void Print(std::ostream& stream) const {
            for (const auto& elem : stack) stream << "[" << elem << "]";
            stream << " ";
        }
    private:
        std::vector<std::string> stack;
    };

    inline std::ostream& operator<<(std::ostream& stream, const StatusStack& statusStack) {
        statusStack.Print(stream);
        return stream;
    }

} // namespace plankton

#endif //PLANKTON_UTIL_LOG_HPP