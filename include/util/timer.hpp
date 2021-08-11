#pragma once
#ifndef PLANKTON_UTIL_TIMER_HPP
#define PLANKTON_UTIL_TIMER_HPP

#include <chrono>
#include <sstream>
#include "log.hpp"

namespace plankton {
    
    class Timer {
    private:
        std::string info;
        std::size_t counter;
        std::chrono::milliseconds elapsed;

        [[nodiscard]] inline std::string ToString(const std::string& note) const {
            std::stringstream stream;
            stream << note << " '" << info << "' (" << counter << "): ";
            auto milli = elapsed.count();
            stream << milli << "ms";
            stream << std::endl;
            return stream.str();
        }

    public:
        class Measurement {
        private:
            Timer& parent;
            std::chrono::time_point<std::chrono::steady_clock> start;

        public:
            Measurement(const Measurement& other) = delete;
            explicit Measurement(Timer& parent) : parent(parent), start(std::chrono::steady_clock::now()) {}

            ~Measurement() {
                auto end = std::chrono::steady_clock::now();
                auto myElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                parent.elapsed += myElapsed;
                parent.counter++;
                // DEBUG("$MEASUREMENT for " << parent.info << ": " << myElapsed.count() << "ms" << std::endl)
            }
        };

        explicit Timer(std::string info) : info(std::move(info)), counter(0), elapsed(0) {}
        ~Timer() { DEBUG(ToString("Total time measured for")) }
        void Print() const { INFO(ToString("Time measured for")) }
        Measurement Measure() { return Measurement(*this); }
    };

} // plankton

#endif //PLANKTON_UTIL_TIMER_HPP