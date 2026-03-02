#pragma once

#ifdef PROFILING_ENABLED
#include <chrono>
#include <string>
#include <iostream>

namespace Boidsish {
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& name) : name_(name), start_(std::chrono::high_resolution_clock::now()) {}
        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start_;
            // Metric-First: Logging for verification of performance improvements
            std::cout << "[PROFILE] " << name_ << ": " << elapsed.count() << " ms" << std::endl;
        }
    private:
        std::string name_;
        std::chrono::time_point<std::chrono::high_resolution_clock> start_;
    };
}

#define PROJECT_PROFILE_SCOPE(name) Boidsish::ScopedTimer timer_##__LINE__(name)
#define PROJECT_MARKER(name) // Marker implementation if needed
#else
#define PROJECT_PROFILE_SCOPE(name)
#define PROJECT_MARKER(name)
#endif
