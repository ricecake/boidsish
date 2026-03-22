#include "profiler.h"

#ifdef PROFILING_ENABLED

namespace Boidsish {

	Profiler& Profiler::GetInstance() {
		static Profiler instance;
		return instance;
	}

	void Profiler::RecordSample(const char* name, double durationUs) {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto&                       stats = m_stats[name];
		stats.count++;
		stats.totalTimeUs += durationUs;
		if (durationUs < stats.minTimeUs)
			stats.minTimeUs = durationUs;
		if (durationUs > stats.maxTimeUs)
			stats.maxTimeUs = durationUs;

		m_frameCalls[name]++;

		// EMA update for time
		const double alpha = 0.05;
		if (stats.emaTimeUs == 0.0) {
			stats.emaTimeUs = durationUs;
		} else {
			stats.emaTimeUs = alpha * durationUs + (1.0 - alpha) * stats.emaTimeUs;
		}
	}

	void Profiler::Update(float deltaTime) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_frameCount++;
		m_totalFrames++;
		m_accumulatedTime += deltaTime;

		// Calculate per-frame metrics
		const double alpha = 0.05;
		for (auto& [name, stats] : m_stats) {
			uint64_t calls = 0;
			if (m_frameCalls.count(name)) {
				calls = m_frameCalls[name];
			}

			stats.lastFrameCalls = calls;
			stats.avgCallsPerFrame = static_cast<double>(stats.count) / m_totalFrames;

			if (m_totalFrames == 1) {
				stats.emaCallsPerFrame = static_cast<double>(calls);
			} else {
				stats.emaCallsPerFrame = alpha * static_cast<double>(calls) + (1.0 - alpha) * stats.emaCallsPerFrame;
			}
		}

		m_frameCalls.clear();

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
		m_frameCalls.clear();
		m_totalFrames = 0;
	}

	ProfileScope::ProfileScope(const char* name): m_name(name), m_start(std::chrono::high_resolution_clock::now()) {}

	ProfileScope::~ProfileScope() {
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
		Profiler::GetInstance().RecordSample(m_name, static_cast<double>(duration));
	}
} // namespace Boidsish

#endif
