#include <cassert>
#include <vector>

#include "terrain_generator.h"

int main() {
	Boidsish::TerrainGenerator terrain_generator;
	for (auto i = 0; i < 4 * 32 * 32; i += 32) {
		std::vector<uint16_t> pixels = terrain_generator.GenerateSuperChunkTexture(0, i);
	}
	// assert(pixels.size() == 128 * 128 * 4);
	// terrain_generator.ConvertDatToPng("terrain_cache/superchunk_0_0.dat", "terrain_cache/superchunk_0_0.png");
	return 0;
}
