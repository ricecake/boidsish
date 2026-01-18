#include "perf_counter.h"

#ifdef ENABLE_PERF_COUNTER

std::map<std::string, double> PerfCounter::scope_times_;
std::map<std::string, int> PerfCounter::counts_;

PerfCounter::PerfCounter(const std::string& name) : name_(name) {
    start_time_ = std::chrono::high_resolution_clock::now();
}

PerfCounter::~PerfCounter() {
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_).count() / 1000.0;
    scope_times_[name_] += duration;
}

void PerfCounter::Count(const std::string& name, int count) {
    counts_[name] += count;
}

std::map<std::string, double> PerfCounter::GetScopeTimes() {
    return scope_times_;
}

std::map<std::string, int> PerfCounter::GetCounts() {
    return counts_;
}

void PerfCounter::Reset() {
    scope_times_.clear();
    counts_.clear();
}

#endif // ENABLE_PERF_COUNTER
