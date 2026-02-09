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
        fractal->SetGain(0.5f);
        fractal->SetLacunarity(2.0f);
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
        float padding = 20.0f; // More padding for noise
        glm::vec3 min_bound = glm::min(start, end) - glm::vec3(padding);
        glm::vec3 max_bound = glm::max(start, end) + glm::vec3(padding);

        return DualContouring::Generate(min_bound, max_bound, cell_size,
            [&](const glm::vec3& p) { return TunnelSDF(p, start, end); });
    }

    float CaveGenerator::CaveSDF(const glm::vec3& p, const glm::vec3& entrance) const {
        // Entrance tube/sphere
        float entrance_dist = glm::length(p - entrance) - 5.0f;

        // Main chamber
        glm::vec3 chamber_center = entrance + glm::vec3(0, -15, 10);
        float n = noise_->GenSingle3D(p.x * 0.08f, p.y * 0.08f, p.z * 0.08f, seed_);
        float chamber = glm::length(p - chamber_center) - (18.0f + n * 6.0f);

        return smin(entrance_dist, chamber, 8.0f);
    }

    float CaveGenerator::TunnelSDF(const glm::vec3& p, const glm::vec3& start, const glm::vec3& end) const {
        glm::vec3 pa = p - start, ba = end - start;
        float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);

        float n = noise_->GenSingle3D(p.x * 0.12f, p.y * 0.12f, p.z * 0.12f, seed_);
        float radius = 6.0f + n * 2.5f;

        return glm::length(pa - ba * h) - radius;
    }

}
