#include "pathfinder.h"

namespace Boidsish {

Pathfinder::Pathfinder(const TerrainGenerator& terrain)
    : _terrain(terrain) {}

std::vector<glm::vec3> Pathfinder::findPathByRefinement(
    const glm::vec3& start,
    const glm::vec3& end,
    int numWaypoints,
    int numIterations,
    int numSubdivisions
) {
    std::vector<glm::vec3> path;
    glm::vec3 direction = end - start;
    float totalDist = glm::length(direction);
    direction = glm::normalize(direction);

    // Initial path generation
    for (int i = 0; i < numWaypoints; ++i) {
        float frac = static_cast<float>(i) / (numWaypoints - 1);
        glm::vec3 point = start + direction * (totalDist * frac);

        // Lifting
        float terrainHeight = std::get<0>(_terrain.pointProperties(point.x, point.z));
        point.y = terrainHeight + 1.0f; // Lift 1 unit above terrain
        path.push_back(point);
    }

    for (int sub = 0; sub < numSubdivisions; ++sub) {
        // Downhill refinement
        for (int iter = 0; iter < numIterations; ++iter) {
            for (size_t i = 1; i < path.size() - 1; ++i) { // Exclude start and end
                glm::vec3& point = path[i];

                // Get terrain normal (derivative information)
                glm::vec3 normal = std::get<1>(_terrain.pointProperties(point.x, point.z));

            // Move "downhill" by moving in the opposite direction of the horizontal gradient
            glm::vec3 horizontal_gradient(normal.x, 0.0f, normal.z);
            if (glm::dot(horizontal_gradient, horizontal_gradient) > 0.000001f) {
                glm::vec3 uphill = glm::normalize(horizontal_gradient);
                point -= uphill * 0.5f; // Step size of 0.5
            }

                // Re-lift
                float terrainHeight = std::get<0>(_terrain.pointProperties(point.x, point.z));
                point.y = terrainHeight + 1.0f;
            }
        }

        // Subdivision
        if (sub < numSubdivisions - 1) { // Don't subdivide on the last run
            std::vector<glm::vec3> newPath;
            newPath.push_back(path.front());
            for (size_t i = 0; i < path.size() - 1; ++i) {
                glm::vec3 midPoint = (path[i] + path[i+1]) * 0.5f;
                float terrainHeight = std::get<0>(_terrain.pointProperties(midPoint.x, midPoint.z));
                midPoint.y = terrainHeight + 1.0f;
                newPath.push_back(midPoint);
                newPath.push_back(path[i+1]);
            }
            path = newPath;
        }
    }

    return path;
}

} // namespace Boidsish
