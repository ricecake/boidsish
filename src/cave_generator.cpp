#include "cave_generator.h"
#include <glm/gtx/norm.hpp>

namespace Boidsish {

    static float smin(float a, float b, float k) {
        float h = glm::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
        return glm::mix(b, a, h) - k * h * (1.0f - h);
    }

    CaveGenerator::CaveGenerator(int seed) : seed_(seed) {
        auto fn = FastNoise::New<FastNoise::Simplex>();
        auto fractal = FastNoise::New<FastNoise::FractalFBm>();
        fractal->SetSource(fn);
        fractal->SetOctaveCount(3);
        noise_ = fractal;
    }

    DualContouringMesh CaveGenerator::GenerateCaveMesh(
        const glm::vec3& entrance_pos,
        float bounds_size,
        float cell_size
    ) {
        glm::vec3 min_bound = entrance_pos - glm::vec3(bounds_size * 0.5f, bounds_size * 0.8f, bounds_size * 0.5f);
        glm::vec3 max_bound = entrance_pos + glm::vec3(bounds_size * 0.5f, bounds_size * 0.2f, bounds_size * 0.5f);

        return DualContouring::Generate(min_bound, max_bound, cell_size,
            [&](const glm::vec3& p) { return CaveSDF(p, entrance_pos); });
    }

    DualContouringMesh CaveGenerator::GenerateTunnelMesh(
        const glm::vec3& start,
        const glm::vec3& end,
        float cell_size
    ) {
        float padding = 15.0f;
        glm::vec3 min_bound = glm::min(start, end) - glm::vec3(padding);
        glm::vec3 max_bound = glm::max(start, end) + glm::vec3(padding);

        return DualContouring::Generate(min_bound, max_bound, cell_size,
            [&](const glm::vec3& p) { return TunnelSDF(p, start, end); });
    }

    float CaveGenerator::CaveSDF(const glm::vec3& p, const glm::vec3& entrance) const {
        float entrance_dist = glm::length(p - entrance) - 4.0f;
        float n = noise_->GenSingle3D(p.x * 0.1f, p.y * 0.1f, p.z * 0.1f, seed_);
        glm::vec3 chamber_center = entrance + glm::vec3(0, -10, 5);
        float chamber = glm::length(p - chamber_center) - (15.0f + n * 5.0f);
        return smin(entrance_dist, chamber, 5.0f);
    }

    float CaveGenerator::TunnelSDF(const glm::vec3& p, const glm::vec3& start, const glm::vec3& end) const {
        glm::vec3 pa = p - start, ba = end - start;
        float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
        float radius = 5.0f;
        float n = noise_->GenSingle3D(p.x * 0.15f, p.y * 0.15f, p.z * 0.15f, seed_);
        radius += n * 1.5f;
        return glm::length(pa - ba * h) - radius;
    }

}
