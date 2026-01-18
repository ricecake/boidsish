#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>

#ifdef ENABLE_PERF_COUNTER

#define PERF_SCOPE(name) PerfCounter perf_counter(name)
#define PERF_COUNT(name, count) PerfCounter::Count(name, count)

class PerfCounter {
public:
    PerfCounter(const std::string& name);
    ~PerfCounter();

    static void Count(const std::string& name, int count);
    static std::map<std::string, double> GetScopeTimes();
    static std::map<std::string, int> GetCounts();
    static void Reset();

private:
    std::string name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;

    static std::map<std::string, double> scope_times_;
    static std::map<std::string, int> counts_;
};

#else // ENABLE_PERF_COUNTER

#define PERF_SCOPE(name)
#define PERF_COUNT(name, count)

class PerfCounter {
public:
    PerfCounter(const std::string& /*name*/) {}
    ~PerfCounter() {}
    static void Count(const std::string& /*name*/, int /*count*/) {}
    static std::map<std::string, double> GetScopeTimes() { return {}; }
    static std::map<std::string, int> GetCounts() { return {}; }
    static void Reset() {}
};

#endif // ENABLE_PERF_COUNTER
