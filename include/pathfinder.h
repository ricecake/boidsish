#pragma once

#include "path.h"
#include "terrain_generator.h"
#include <glm/glm.hpp>
#include <vector>

namespace Boidsish {

class Pathfinder {
public:
    Pathfinder(const TerrainGenerator& terrain);

    std::vector<glm::vec3> findPath(const glm::vec3& start, const glm::vec3& end);
    void smoothPath(std::vector<glm::vec3>& path);

private:
    const TerrainGenerator& _terrain;
};

}
