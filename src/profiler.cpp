#include "profiler.h"

#ifdef PROFILING_ENABLED

namespace Boidsish {

    Profiler& Profiler::GetInstance() {
        static Profiler instance;
        return instance;
    }

    void Profiler::RecordSample(const char* name, double durationUs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& stats = m_stats[name];
        stats.count++;
        stats.totalTimeUs += durationUs;
        if (durationUs < stats.minTimeUs) stats.minTimeUs = durationUs;
        if (durationUs > stats.maxTimeUs) stats.maxTimeUs = durationUs;
    }

    void Profiler::Update(float deltaTime) {
        m_frameCount++;
        m_accumulatedTime += deltaTime;
        if (m_accumulatedTime >= 1.0f) {
            m_fps = static_cast<float>(m_frameCount) / m_accumulatedTime;
            m_frameCount = 0;
            m_accumulatedTime = 0.0f;
        }
    }

    float Profiler::GetFPS() const {
        return m_fps;
    }

    std::map<std::string, ProfileStats> Profiler::GetStats() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }

    void Profiler::ClearStats() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.clear();
    }

    ProfileScope::ProfileScope(const char* name) :
        m_name(name),
        m_start(std::chrono::high_resolution_clock::now()) {}

    ProfileScope::~ProfileScope() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
        Profiler::GetInstance().RecordSample(m_name, static_cast<double>(duration));
    }
}

#endif
