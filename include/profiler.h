#pragma once

#include <GL/glew.h>
#include <string>

#ifdef PROFILING_ENABLED

namespace Boidsish {
    class ProfileScope {
    public:
        ProfileScope(const char* name) {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
        }
        ~ProfileScope() {
            glPopDebugGroup();
        }
    };
}

#define PROJECT_PROFILE_SCOPE(name) Boidsish::ProfileScope _profile_scope_##__LINE__(name)
#define PROJECT_MARKER(name) glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, name)

#else

#define PROJECT_PROFILE_SCOPE(name)
#define PROJECT_MARKER(name)

#endif
