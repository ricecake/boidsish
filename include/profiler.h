#pragma once

/**
 * @file profiler.h
 * @brief Zero-overhead instrumentation system.
 *
 * All profiling calls are wrapped in macros that resolve to no-ops
 * unless PROFILING_ENABLED is defined at compile time.
 */

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifdef PROFILING_ENABLED

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b)       PROFILE_CONCAT_INNER(a, b)

namespace Boidsish {

	/**
	 * @brief Statistics for a single profile scope.
	 */
	struct ProfileStats {
		uint64_t count = 0;
		double   totalTimeUs = 0.0;
		double   minTimeUs = 1e30;
		double   maxTimeUs = 0.0;

		// New metrics
		uint64_t lastFrameCalls = 0;
		double   avgCallsPerFrame = 0.0;
		double   emaTimeUs = 0.0;
		double   emaCallsPerFrame = 0.0;
		double   impact = 0.0;

		double GetAverageUs() const { return count > 0 ? totalTimeUs / count : 0.0; }
	};

	/**
	 * @brief Centralized profiler manager.
	 */
	class Profiler {
	public:
		static Profiler& GetInstance();

		void RecordSample(const char* name, double durationUs);
		void PushScope(const char* name);
		void PopScope();

		void                                Update(float deltaTime);
		float                               GetFPS() const;
		std::map<std::string, ProfileStats> GetStats();
		void                                ClearStats();
		void                                SaveReport();

	private:
		Profiler() = default;
		~Profiler() = default;
		Profiler(const Profiler&) = delete;
		Profiler& operator=(const Profiler&) = delete;

		std::map<std::string, ProfileStats> m_stats;
		std::map<std::string, uint64_t>     m_frameCalls;
		mutable std::mutex                  m_mutex;

		float    m_fps = 0.0f;
		uint32_t m_frameCount = 0;
		float    m_accumulatedTime = 0.0f;
		uint64_t m_totalFrames = 0;
	};

	/**
	 * @brief RAII helper for timing a scope.
	 */
	class ProfileScope {
	public:
		explicit ProfileScope(const char* name);
		~ProfileScope();

	private:
		const char*                                    m_name;
		std::chrono::high_resolution_clock::time_point m_start;
	};
} // namespace Boidsish

	/**
     * @brief Measures the execution time of the current scope.
     * @param name A string literal identifying the scope.
     */
	#define PROJECT_PROFILE_SCOPE(name) Boidsish::ProfileScope PROFILE_CONCAT(profileScope, __LINE__)(name)

	/**
     * @brief Records a point-in-time event marker.
     * @param name A string literal identifying the marker.
     */
	#define PROJECT_MARKER(name)                                                                                       \
		do {                                                                                                           \
			(void)(name);                                                                                              \
		} while (0)

#else

namespace Boidsish {

	/**
	 * @brief Statistics for a single profile scope (stub when disabled).
	 */
	struct ProfileStats {
		uint64_t count = 0;
		double   totalTimeUs = 0.0;
		double   minTimeUs = 0.0;
		double   maxTimeUs = 0.0;

		// Stub metrics for consistency
		uint64_t lastFrameCalls = 0;
		double   avgCallsPerFrame = 0.0;
		double   emaTimeUs = 0.0;
		double   emaCallsPerFrame = 0.0;
		double   impact = 0.0;

		double GetAverageUs() const { return 0.0; }
	};

	class Profiler {
	public:
		static Profiler& GetInstance() {
			static Profiler instance;
			return instance;
		}

		void RecordSample(const char*, double) {}
		void PushScope(const char*) {}
		void PopScope() {}

		void                                Update(float) {}
		float                               GetFPS() const { return 0.0f; }
		std::map<std::string, ProfileStats> GetStats() { return {}; }
		void                                ClearStats() {}
		void                                SaveReport() {}
	};
} // namespace Boidsish

	// Zero-overhead resolution when profiling is disabled
	#define PROJECT_PROFILE_SCOPE(name)                                                                                \
		do {                                                                                                           \
			(void)(name);                                                                                              \
		} while (0)
	#define PROJECT_MARKER(name)                                                                                       \
		do {                                                                                                           \
			(void)(name);                                                                                              \
		} while (0)

#endif
