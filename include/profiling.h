#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include <GL/glew.h>

#ifdef PROFILING_ENABLED

namespace Boidsish {
	namespace Profiling {

		class ProfileScope {
		public:
			ProfileScope(const char* name): m_name(name) {
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
				m_start = std::chrono::high_resolution_clock::now();
			}

			~ProfileScope() {
				auto end = std::chrono::high_resolution_clock::now();
				glPopDebugGroup();

				std::chrono::duration<float, std::milli> duration = end - m_start;
				// In a real high-performance engine, we'd buffer these and display in a UI
			}

		private:
			const char*                                            m_name;
			std::chrono::high_resolution_clock::time_point m_start;
		};

		inline void Marker(const char* name) {
			glDebugMessageInsert(
				GL_DEBUG_SOURCE_APPLICATION,
				GL_DEBUG_TYPE_MARKER,
				0,
				GL_DEBUG_SEVERITY_NOTIFICATION,
				-1,
				name
			);
		}
	} // namespace Profiling
} // namespace Boidsish

	#define PROJECT_PROFILE_SCOPE(name) Boidsish::Profiling::ProfileScope _profile_scope_##__LINE__(name)
	#define PROJECT_MARKER(name) Boidsish::Profiling::Marker(name)

#else

	#define PROJECT_PROFILE_SCOPE(name) \
		do {                            \
		} while (0)
	#define PROJECT_MARKER(name) \
		do {                     \
		} while (0)

#endif
