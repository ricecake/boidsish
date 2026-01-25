#pragma once

#include <glm/glm.hpp>

namespace glm {
    inline bool operator<(const glm::vec2& a, const glm::vec2& b) {
        if (a.x < b.x) return true;
        if (a.x > b.x) return false;
        return a.y < b.y;
    }
}
