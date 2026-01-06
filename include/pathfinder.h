#pragma once

#include "path.h"
#include "terrain_generator.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>

namespace Boidsish {

// Hash for glm::ivec2 for use in unordered_set
struct IVec2Hash {
    std::size_t operator()(const glm::ivec2& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }
};

class Pathfinder {
public:
    Pathfinder(const TerrainGenerator& terrain);

    std::vector<glm::vec3> findPath(const glm::vec3& start, const glm::vec3& end, const std::unordered_set<glm::ivec2, IVec2Hash>& chunkCorridor);
    void smoothPath(std::vector<glm::vec3>& path);

private:
    const TerrainGenerator& _terrain;
};

}
