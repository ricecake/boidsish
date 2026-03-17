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
            return distances[z * res.y * res.x + y * res.x + x];
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
