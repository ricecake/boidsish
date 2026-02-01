#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>

#ifdef PROFILING_ENABLED

namespace Boidsish {
    namespace Profiler {

        struct ProfileStats {
            double total_ms = 0;
            double max_ms = 0;
            uint64_t count = 0;
        };

        class ProfileManager {
        public:
            static ProfileManager& GetInstance() {
                static ProfileManager instance;
                return instance;
            }

            void AddResult(const char* name, double duration_ms) {
                std::lock_guard<std::mutex> lock(mutex_);
                auto& stats = stats_[name];
                stats.total_ms += duration_ms;
                stats.max_ms = std::max(stats.max_ms, duration_ms);
                stats.count++;
            }

            std::map<std::string, ProfileStats> GetStats() {
                std::lock_guard<std::mutex> lock(mutex_);
                return stats_;
            }

            void Clear() {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.clear();
            }

        private:
            ProfileManager() = default;
            std::map<std::string, ProfileStats> stats_;
            std::mutex mutex_;
        };

        class Timer {
        public:
            Timer(const char* name) : name_(name), start_time_(std::chrono::high_resolution_clock::now()) {}

            ~Timer() {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_).count();
                ProfileManager::GetInstance().AddResult(name_, duration / 1000.0);
            }

        private:
            const char* name_;
            std::chrono::high_resolution_clock::time_point start_time_;
        };
    }
}

#define PROJECT_CONCAT_IMPL(x, y) x##y
#define PROJECT_CONCAT(x, y) PROJECT_CONCAT_IMPL(x, y)
#define PROJECT_PROFILE_SCOPE(name) Boidsish::Profiler::Timer PROJECT_CONCAT(timer_, __COUNTER__)(name)
#define PROJECT_MARKER(name) do {} while(0)

#else

#define PROJECT_PROFILE_SCOPE(name) do {} while(0)
#define PROJECT_MARKER(name) do {} while(0)

#endif
