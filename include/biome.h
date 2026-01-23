#pragma once

namespace Boidsish {
    struct BiomeInfo {
        alignas(16) int biome_indices[2];
        alignas(4) float blend;
    };
}
