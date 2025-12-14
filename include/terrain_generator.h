#pragma once

#include <memory>
#include <vector>
#include <map>
#include "terrain.h"
#include "PerlinNoise.hpp"
#include "graphics.h"

namespace Boidsish {

class TerrainGenerator {
public:
    TerrainGenerator(int seed = 12345);

    void update(const Camera& camera);
    std::vector<std::shared_ptr<Terrain>> getVisibleChunks();

private:
    std::shared_ptr<Terrain> generateChunk(int chunkX, int chunkZ);

    // Terrain parameters
    const int chunk_size_ = 32;
    const float scale_ = 0.1f;
    const float amplitude_ = 5.0f;
    const float threshold_ = 0.5f;
    const int view_distance_ = 4; // in chunks

    // Noise generator
    siv::PerlinNoise perlin_noise_;

    // Cache
    std::map<std::pair<int, int>, std::shared_ptr<Terrain>> chunk_cache_;
};

} // namespace Boidsish
