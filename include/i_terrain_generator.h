#pragma once

#include <memory>
#include <tuple>
#include <vector>

#include "terrain.h"
#include <glm/glm.hpp>

namespace Boidsish {
    struct Frustum;
    struct Camera;
}

namespace Boidsish {

class ITerrainGenerator {
public:
    virtual ~ITerrainGenerator() = default;

    virtual void update(const Frustum& frustum, const Camera& camera) = 0;
    virtual const std::vector<std::shared_ptr<Terrain>>& getVisibleChunks() const = 0;

    virtual std::vector<uint16_t> GenerateSuperChunkTexture(int requested_x, int requested_z) = 0;
    virtual std::vector<uint16_t> GenerateTextureForArea(int world_x, int world_z, int size) = 0;
    virtual void ConvertDatToPng(const std::string& dat_filepath, const std::string& png_filepath) = 0;

    virtual float GetMaxHeight() const = 0;

    virtual std::tuple<float, glm::vec3> pointProperties(float x, float z) const = 0;

    virtual bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, float& out_dist) const = 0;

    virtual std::vector<glm::vec3> GetPath(glm::vec2 start_pos, int num_points, float step_size) const = 0;

    virtual float getBiomeControlValue(float x, float z) const = 0;
    virtual glm::vec2 getDomainWarp(float x, float z) const = 0;
};

}