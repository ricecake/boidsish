#include "terrain_field.h"

#include <glm/glm.hpp>
#include <numeric>

namespace Boidsish {

TerrainField::TerrainField(const std::vector<std::shared_ptr<Terrain>>& terrains, float influence_radius)
    : lut_(influence_radius) {
    for (const auto& terrain : terrains) {
        Patch patch;
        patch.vertices = terrain->getVertices();
        patch.normals = terrain->getNormals();

        if (patch.vertices.empty()) continue;

        // Compute center
        glm::vec3 sum_pos(0.0f);
        for (const auto& v : patch.vertices) {
            sum_pos += v;
        }
        patch.proxy.center = sum_pos / (float)patch.vertices.size();

        // Compute total normal and Z bounds
        patch.proxy.totalNormal = glm::vec3(0.0f);
        patch.proxy.minZ = std::numeric_limits<float>::max();
        patch.proxy.maxZ = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < patch.vertices.size(); ++i) {
            patch.proxy.totalNormal += patch.normals[i];
            patch.proxy.minZ = std::min(patch.proxy.minZ, patch.vertices[i].z);
            patch.proxy.maxZ = std::max(patch.proxy.maxZ, patch.vertices[i].z);
        }

        // Compute bounding radius
        patch.proxy.radiusSq = 0.0f;
        for (const auto& v : patch.vertices) {
            float dist = glm::distance(v, patch.proxy.center);
            float distSq = dist * dist;
            if (distSq > patch.proxy.radiusSq) {
                patch.proxy.radiusSq = distSq;
            }
        }
        patches_.push_back(patch);
    }
}

Vector3 TerrainField::getInfluence(const Vector3& position) const {
    glm::vec3 total_influence(0.0f);

    struct BirdLike {
        glm::vec3 position;
        glm::vec3 forceAccumulator;
    } bird;
    bird.position = glm::vec3(position.x, position.y, position.z);


    for (const auto& patch : patches_) {
        patch.proxy.ApplyPatchInfluence(bird, patch, lut_);
    }

    total_influence = bird.forceAccumulator;

    return Vector3(total_influence.x, total_influence.y, total_influence.z);
}

}
