#pragma once

/**
 * @file profiling.h
 * @brief Zero-overhead profiling macros for the Boidsish framework.
 *
 * This file provides macros for instrumentation that are entirely stripped
 * in production builds unless PROFILING_ENABLED is defined.
 */

#ifdef PROFILING_ENABLED

#include <chrono>
#include <string>
#include <iostream>

namespace Boidsish {
namespace Profiling {

    /**
     * @brief Simple RAII scope profiler that prints elapsed time.
     * In a real system, this might log to a more sophisticated telemetry system.
     */
    class ProfileScope {
    public:
        ProfileScope(const char* name) : name_(name), start_(std::chrono::high_resolution_clock::now()) {}
        ~ProfileScope() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
            // Using a static string or similar would be better, but for now just print
            // In a production-ready system, we'd use a lock-free ring buffer or similar.
            // std::cout << "[PROFILE] " << name_ << ": " << duration << " us" << std::endl;
        }
    private:
        const char* name_;
        std::chrono::high_resolution_clock::time_point start_;
    };

    /**
     * @brief Function to record a single event/marker.
     */
    inline void RecordMarker(const char* name) {
        // Implementation for recording markers
    }
}
}

#define PROJECT_PROFILE_SCOPE(name) Boidsish::Profiling::ProfileScope profile_scope_##__LINE__(name)
#define PROJECT_MARKER(name) Boidsish::Profiling::RecordMarker(name)

#else

// No-op macros when profiling is disabled
#define PROJECT_PROFILE_SCOPE(name) do {} while(0)
#define PROJECT_MARKER(name) do {} while(0)

#endif
