#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "geometry.h"

namespace Boidsish {

    class Model;

    struct SdfData {
        glm::uvec3 res;
        glm::vec3 aabb_min;
        glm::vec3 aabb_max;
        std::vector<float> distances;

        float GetDistance(uint32_t x, uint32_t y, uint32_t z) const {
            x = std::clamp(x, 0u, res.x - 1);
            y = std::clamp(y, 0u, res.y - 1);
            z = std::clamp(z, 0u, res.z - 1);
            return distances[z * res.y * res.x + y * res.x + x];
        }

        float Sample(glm::vec3 world_pos) const {
            glm::vec3 size = aabb_max - aabb_min;
            glm::vec3 local_pos = (world_pos - aabb_min) / size * glm::vec3(res - glm::uvec3(1));

            if (world_pos.x < aabb_min.x || world_pos.x > aabb_max.x ||
                world_pos.y < aabb_min.y || world_pos.y > aabb_max.y ||
                world_pos.z < aabb_min.z || world_pos.z > aabb_max.z) {
                // Return a large positive value for positions far outside,
                // or approximate with distance to AABB
                glm::vec3 d = glm::max(aabb_min - world_pos, world_pos - aabb_max);
                return glm::length(glm::max(d, 0.0f));
            }

            glm::uvec3 base = glm::uvec3(glm::floor(local_pos));
            glm::vec3 frac = local_pos - glm::vec3(base);

            auto g = [&](uint32_t x, uint32_t y, uint32_t z) {
                return GetDistance(base.x + x, base.y + y, base.z + z);
            };

            float c00 = g(0, 0, 0) * (1.0f - frac.x) + g(1, 0, 0) * frac.x;
            float c01 = g(0, 0, 1) * (1.0f - frac.x) + g(1, 0, 1) * frac.x;
            float c10 = g(0, 1, 0) * (1.0f - frac.x) + g(1, 1, 0) * frac.x;
            float c11 = g(0, 1, 1) * (1.0f - frac.x) + g(1, 1, 1) * frac.x;

            float c0 = c00 * (1.0f - frac.y) + c10 * frac.y;
            float c1 = c01 * (1.0f - frac.y) + c11 * frac.y;

            return c0 * (1.0f - frac.z) + c1 * frac.z;
        }
    };

    struct VoxelGrid {
        glm::uvec3 res;
        glm::vec3 aabb_min;
        glm::vec3 aabb_max;
        glm::vec3 voxel_size;
        std::vector<uint8_t> voxels; // 0: empty, 1: surface, 2: interior

        VoxelGrid(glm::uvec3 resolution, glm::vec3 min, glm::vec3 max)
            : res(resolution), aabb_min(min), aabb_max(max) {
            voxels.resize(res.x * res.y * res.z, 0);
            voxel_size = (aabb_max - aabb_min) / glm::vec3(res);
        }

        uint8_t Get(uint32_t x, uint32_t y, uint32_t z) const {
            return voxels[z * res.y * res.x + y * res.x + x];
        }

        void Set(uint32_t x, uint32_t y, uint32_t z, uint8_t val) {
            voxels[z * res.y * res.x + y * res.x + x] = val;
        }

        glm::vec3 GetVoxelCenter(uint32_t x, uint32_t y, uint32_t z) const {
            return aabb_min + (glm::vec3(x, y, z) + 0.5f) * voxel_size;
        }
    };

    class SdfUtils {
    public:
        static VoxelGrid VoxelizeModel(const Model& model, glm::uvec3 resolution);
        static SdfData GenerateSDF(const VoxelGrid& grid);

        static bool SaveSDF(const SdfData& data, const std::string& path);
        static bool LoadSDF(SdfData& data, const std::string& path);
    };

} // namespace Boidsish
