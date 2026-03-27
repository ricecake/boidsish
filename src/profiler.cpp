#include "profiler.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <stack>
#include <cstring>

#ifdef PROFILING_ENABLED

namespace Boidsish {

	static thread_local std::stack<const char*> s_scopeStack;
	static thread_local std::string             s_currentPath;

	Profiler& Profiler::GetInstance() {
		static Profiler instance;
		return instance;
	}

	void Profiler::RecordSample(const char* name, double durationUs) {
		std::string fullName = s_currentPath;
		if (name) {
			if (!fullName.empty())
				fullName += "/";
			fullName += name;
		}

		if (fullName.empty()) {
			// If we're not in a path and have no name, use "unknown"
			fullName = "unknown";
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		auto&                       stats = m_stats[fullName];
		stats.count++;
		stats.totalTimeUs += durationUs;
		if (durationUs < stats.minTimeUs)
			stats.minTimeUs = durationUs;
		if (durationUs > stats.maxTimeUs)
			stats.maxTimeUs = durationUs;

		m_frameCalls[fullName]++;

		// EMA update for time
		const double alpha = 0.05;
		if (stats.emaTimeUs == 0.0) {
			stats.emaTimeUs = durationUs;
		} else {
			stats.emaTimeUs = alpha * durationUs + (1.0 - alpha) * stats.emaTimeUs;
		}
	}

	void Profiler::PushScope(const char* name) {
		const char* actualName = name ? name : "unknown";
		if (!s_currentPath.empty()) {
			s_currentPath += "/";
		}
		s_currentPath += actualName;
		s_scopeStack.push(actualName);
	}

	void Profiler::PopScope() {
		if (s_scopeStack.empty())
			return;

		const char* topName = s_scopeStack.top();
		s_scopeStack.pop();

		size_t nameLen = strlen(topName);
		if (s_currentPath.length() > nameLen) {
			s_currentPath.erase(s_currentPath.length() - nameLen - 1); // Remove name and the slash
		} else {
			s_currentPath.clear();
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

			stats.impact = stats.emaTimeUs * stats.emaCallsPerFrame;
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

	void Profiler::SaveReport() {
		std::lock_guard<std::mutex> lock(m_mutex);

		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);

		std::ostringstream oss;
		oss << "profile_" << BOIDSISH_BUILD_ID << "_";
		oss << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".csv";
		std::string filename = oss.str();

		std::ofstream file(filename);
		if (!file.is_open())
			return;

		file << "Build ID," << BOIDSISH_BUILD_ID << "\n";
		file << "Timestamp," << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n\n";

		file << "Name,Count,Avg (us),EMA (us),Min (us),Max (us),Avg Calls/F,EMA Calls/F,Impact\n";
		for (const auto& [name, stats] : m_stats) {
			file << "\"" << name << "\"," << stats.count << "," << (stats.totalTimeUs / stats.count) << ","
				 << stats.emaTimeUs << "," << stats.minTimeUs << "," << stats.maxTimeUs << "," << stats.avgCallsPerFrame
				 << "," << stats.emaCallsPerFrame << "," << stats.impact << "\n";
		}
	}

	ProfileScope::ProfileScope(const char* name): m_name(name), m_start(std::chrono::high_resolution_clock::now()) {
		Profiler::GetInstance().PushScope(name);
	}

	ProfileScope::~ProfileScope() {
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
		Profiler::GetInstance().RecordSample(nullptr, static_cast<double>(duration));
		Profiler::GetInstance().PopScope();
	}
} // namespace Boidsish

#endif
