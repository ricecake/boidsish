#include "cave_generator.h"
#include <glm/gtx/norm.hpp>
#include <algorithm>

namespace Boidsish {

    static float smin(float a, float b, float k) {
        float h = glm::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
        return glm::mix(b, a, h) - k * h * (1.0f - h);
    }

    static float sdBox(const glm::vec3& p, const glm::vec3& b) {
        glm::vec3 q = glm::abs(p) - b;
        return glm::length(glm::max(q, 0.0f)) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
    }

    CaveGenerator::CaveGenerator(int seed) : seed_(seed) {
        auto fn = FastNoise::New<FastNoise::Simplex>();
        auto fractal = FastNoise::New<FastNoise::FractalFBm>();
        fractal->SetSource(fn);
        fractal->SetOctaveCount(4);
        fractal->SetGain(0.45f);
        fractal->SetLacunarity(2.0f);
        noise_ = fractal;
    }

    DualContouringMesh CaveGenerator::GenerateCaveMesh(
        const glm::vec3& entrance_pos,
        float bounds_size,
        float cell_size
    ) {
        glm::vec3 min_bound = entrance_pos - glm::vec3(bounds_size * 0.5f, bounds_size * 1.2f, bounds_size * 0.5f);
        glm::vec3 max_bound = entrance_pos + glm::vec3(bounds_size * 0.5f, bounds_size * 0.2f, bounds_size * 0.5f);

        return DualContouring::Generate(min_bound, max_bound, cell_size,
            [&](const glm::vec3& p) {
                float d = CaveSDF(p, entrance_pos);

                // Closure constraint: ensure mesh is solid at boundaries
                glm::vec3 center = (min_bound + max_bound) * 0.5f;
                glm::vec3 half_size = (max_bound - min_bound) * 0.5f - glm::vec3(cell_size * 1.5f);
                float b = sdBox(p - center, half_size);
                return std::max(d, b);
            });
    }

    DualContouringMesh CaveGenerator::GenerateTunnelMesh(
        const glm::vec3& start,
        const glm::vec3& end,
        float cell_size
    ) {
        float padding = 25.0f;
        glm::vec3 min_bound = glm::min(start, end) - glm::vec3(padding);
        glm::vec3 max_bound = glm::max(start, end) + glm::vec3(padding);

        return DualContouring::Generate(min_bound, max_bound, cell_size,
            [&](const glm::vec3& p) {
                float d = TunnelSDF(p, start, end);
                glm::vec3 center = (min_bound + max_bound) * 0.5f;
                glm::vec3 half_size = (max_bound - min_bound) * 0.5f - glm::vec3(cell_size * 1.5f);
                float b = sdBox(p - center, half_size);
                return std::max(d, b);
            });
    }

    float CaveGenerator::CaveSDF(const glm::vec3& p, const glm::vec3& entrance) const {
        // Organic chamber
        glm::vec3 chamber_center = entrance + glm::vec3(0, -25, 10);
        float n = noise_->GenSingle3D(p.x * 0.06f, p.y * 0.06f, p.z * 0.06f, seed_);
        float chamber = glm::length(p - chamber_center) - (22.0f + n * 10.0f);

        // Entrance tunnel
        glm::vec3 pa = p - entrance, ba = chamber_center - entrance;
        float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
        float tube = glm::length(pa - ba * h) - 6.0f;

        // Entrance hole blend
        float ent = glm::length(p - entrance) - 8.0f;

        return smin(smin(chamber, tube, 8.0f), ent, 6.0f);
    }

    float CaveGenerator::TunnelSDF(const glm::vec3& p, const glm::vec3& start, const glm::vec3& end) const {
        glm::vec3 pa = p - start, ba = end - start;
        float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);

        float n = noise_->GenSingle3D(p.x * 0.1f, p.y * 0.1f, p.z * 0.1f, seed_);
        float radius = 7.0f + n * 4.0f;

        return glm::length(pa - ba * h) - radius;
    }

}
