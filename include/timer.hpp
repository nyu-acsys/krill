#pragma once
#ifndef TIMER
#define TIMER


class Timer {
    private:
        std::string info;
        std::chrono::milliseconds elapsed;

        inline void Print(const std::string& note) const {
            std::cout << note << " '" << info << "': ";
            auto milli = elapsed.count();
            // if (milli < 1000) std::cout << milli << "ms";
            // else std::cout << ((milli/100)/10.0) << "s";
            std::cout << milli << "ms";
            std::cout << std::endl;
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
//                    std::cout << "$MEASUREMENT for " << parent.info << ": " << myElapsed.count() << "ms" << std::endl;
                }
        };

        explicit Timer(std::string info) : info(std::move(info)), elapsed(0) {}
        ~Timer() { Print("Total time measured for"); }
        void Print() const { Print("Time measured for"); }
        Measurement Measure() { return Measurement(*this); }
};

#endif