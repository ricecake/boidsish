#include "terrain_generator.h"
#include <cassert>
#include <vector>

int main() {
    Boidsish::TerrainGenerator terrain_generator;
    std::vector<uint16_t> pixels = terrain_generator.GenerateSuperChunkTexture(0, 0);
    assert(pixels.size() == 128 * 128 * 4);
    terrain_generator.ConvertDatToPng("terrain_cache/superchunk_0_0.dat", "terrain_cache/superchunk_0_0.png");
    return 0;
}
