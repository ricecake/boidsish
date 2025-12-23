#include "terrain_field.h"

#include "graphics.h"
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

        glm::vec3 terrain_pos(terrain->GetX(), terrain->GetY(), terrain->GetZ());
        for (auto& v : patch.vertices) {
            v += terrain_pos;
        }

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
        patch.proxy.totalNormal = glm::normalize(patch.proxy.totalNormal);

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

Vector3 TerrainField::getInfluence(const Vector3& position, Visualizer& viz) const {
    glm::vec3 pos(position.x, position.y, position.z);
    glm::vec3 total_force(0.0f);

    // Repulsion force
    for (const auto& patch : patches_) {
        glm::vec3 delta = pos - patch.proxy.center;
        float     dist_sq = glm::dot(delta, delta);

        float combined_radius = lut_.R + std::sqrt(patch.proxy.radiusSq);
        if (dist_sq > (combined_radius * combined_radius)) {
            continue;
        }

        if (dist_sq > patch.proxy.radiusSq * 4.0f) {
            total_force -= lut_.Sample(delta, dist_sq, patch.proxy.totalNormal);
        } else {
            for (size_t i = 0; i < patch.vertices.size(); ++i) {
                glm::vec3 r_vec = pos - patch.vertices[i];
                float     r2 = glm::dot(r_vec, r_vec);
                if (r2 < lut_.R * lut_.R) {
                    total_force -= lut_.Sample(r_vec, r2, patch.normals[i]);
                }
            }
        }
    }

    // Tangential (gliding) force
    auto [height, normal] = viz.GetTerrainProperties(pos.x, pos.z);
    glm::vec3 gravity(0.0f, -0.1f, 0.0f);
    glm::vec3 tangent_force = gravity - glm::dot(gravity, normal) * normal;
    total_force += tangent_force;

    return Vector3(total_force.x, total_force.y, total_force.z);
}

}
