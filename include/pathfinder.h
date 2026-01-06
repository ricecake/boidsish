#pragma once

#include "path.h"
#include "terrain_generator.h"
#include <glm/glm.hpp>
#include <vector>

namespace Boidsish {

class Pathfinder {
public:
    Pathfinder(const TerrainGenerator& terrain);

    std::vector<glm::vec3> findPathByRefinement(
        const glm::vec3& start,
        const glm::vec3& end,
        int numWaypoints,
        int numIterations,
        int numSubdivisions
    );

private:
    const TerrainGenerator& _terrain;
};

} // namespace Boidsish
