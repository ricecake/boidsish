#pragma once

/**
 * @file profiler.h
 * @brief Zero-overhead instrumentation system.
 *
 * All profiling calls are wrapped in macros that resolve to no-ops
 * unless PROFILING_ENABLED is defined at compile time.
 */

#ifdef PROFILING_ENABLED
#include <chrono>
#include <string>
#include <iostream>
#include "logger.h"

namespace Boidsish {
    /**
     * @brief RAII helper for timing a scope.
     */
    class ProfileScope {
    public:
        explicit ProfileScope(const char* name) :
            m_name(name),
            m_start(std::chrono::high_resolution_clock::now()) {}

        ~ProfileScope() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();

            // Simple logging for now. In a full system, this would go to a telemetry buffer.
            // Using logger::DEBUG to avoid cluttering normal output.
            // logger::DEBUG("PROFILE | {} | {} us", m_name, duration);
        }

    private:
        const char* m_name;
        std::chrono::high_resolution_clock::time_point m_start;
    };
}

/**
 * @brief Measures the execution time of the current scope.
 * @param name A string literal identifying the scope.
 */
#define PROJECT_PROFILE_SCOPE(name) Boidsish::ProfileScope profileScope##__LINE__(name)

/**
 * @brief Records a point-in-time event marker.
 * @param name A string literal identifying the marker.
 */
#define PROJECT_MARKER(name) do { (void)(name); } while(0)

#else

// Zero-overhead resolution when profiling is disabled
#define PROJECT_PROFILE_SCOPE(name) do { (void)(name); } while(0)
#define PROJECT_MARKER(name) do { (void)(name); } while(0)

#endif
