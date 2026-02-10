#pragma once

#include "dual_contouring.h"
#include <FastNoise/FastNoise.h>
#include <memory>

namespace Boidsish {

    class CaveGenerator {
    public:
        CaveGenerator(int seed = 12345);

        /**
         * @brief Generate a cave mesh.
         * @param entrance_pos World position of the entrance.
         * @param bounds_size Size of the generation volume.
         */
        DualContouringMesh GenerateCaveMesh(
            const glm::vec3& entrance_pos,
            float bounds_size = 50.0f,
            float cell_size = 1.0f
        );

        /**
         * @brief Generate a tunnel mesh through a mountain.
         */
        DualContouringMesh GenerateTunnelMesh(
            const glm::vec3& start,
            const glm::vec3& end,
            float cell_size = 1.5f
        );

    private:
        float CaveSDF(const glm::vec3& p, const glm::vec3& entrance) const;
        float TunnelSDF(const glm::vec3& p, const glm::vec3& start, const glm::vec3& end) const;

        FastNoise::SmartNode<> noise_;
        int seed_;
    };

}
