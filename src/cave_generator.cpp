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
                glm::vec3 half_size = (max_bound - min_bound) * 0.5f - glm::vec3(cell_size * 0.5f);
                float b = sdBox(p - center, half_size);
                return std::max(d, b);
            });
    }

    DualContouringMesh CaveGenerator::GenerateTunnelMesh(
        const glm::vec3& start,
        const glm::vec3& end,
        float cell_size
    ) {
        // Extend bounds above terrain to ensure entrances connect properly
        float padding = 15.0f;
        float vertical_padding = 20.0f;  // Extra vertical space for entrances

        glm::vec3 min_bound = glm::min(start, end) - glm::vec3(padding, padding, padding);
        glm::vec3 max_bound = glm::max(start, end) + glm::vec3(padding, vertical_padding, padding);

        // Store start/end for the SDF lambda
        glm::vec3 s = start;
        glm::vec3 e = end;

        return DualContouring::Generate(min_bound, max_bound, cell_size,
            [&, s, e](const glm::vec3& p) {
                float d = TunnelSDF(p, s, e);

                // Closure constraint with cutouts at entrance positions
                glm::vec3 center = (min_bound + max_bound) * 0.5f;
                glm::vec3 half_size = (max_bound - min_bound) * 0.5f - glm::vec3(cell_size * 0.5f);
                float b = sdBox(p - center, half_size);

                // Create entrance cutouts - don't apply closure near tunnel entrances
                float entrance_radius = 10.0f;
                float start_dist = glm::length(p - s);
                float end_dist = glm::length(p - e);

                // Near entrances, use only the tunnel SDF (no closure)
                float entrance_blend = glm::min(
                    glm::smoothstep(entrance_radius * 0.5f, entrance_radius * 1.5f, start_dist),
                    glm::smoothstep(entrance_radius * 0.5f, entrance_radius * 1.5f, end_dist)
                );

                return glm::mix(d, std::max(d, b), entrance_blend);
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
        float base_radius = 7.0f + n * 3.0f;

        // Main tunnel tube
        float tube = glm::length(pa - ba * h) - base_radius;

        // Entrance spheres at both ends - creates openings that match terrain holes
        float entrance_radius = 8.0f;
        float start_sphere = glm::length(p - start) - entrance_radius;
        float end_sphere = glm::length(p - end) - entrance_radius;

        // Blend entrances smoothly with tube
        return smin(smin(tube, start_sphere, 4.0f), end_sphere, 4.0f);
    }

}
