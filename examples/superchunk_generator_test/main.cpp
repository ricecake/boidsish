#include <cassert>
#include <vector>

#include "logger.h"
#include "terrain_generator.h"

int main() {
	Boidsish::TerrainGenerator terrain_generator;
	for (auto i = 0; i < 4 * 32 * 32; i += 32) {
		std::vector<uint16_t> pixels = terrain_generator.GenerateSuperChunkTexture(0, 0);
		assert(pixels.size() == 128 * 128 * 4);
		auto conv = terrain_generator.SuperChunkTextureToVec(pixels);

		for (auto i = 0; i < 20; i++) {
			logger::LOG("DATA", std::get<0>(conv.at(i)));
			auto [height, normal] = terrain_generator.pointProperties(i, 0);
			logger::LOG("expe", height);
		}
	}
	terrain_generator.ConvertDatToPng("terrain_cache/superchunk_0.dat", "terrain_cache/superchunk_0.png");

	return 0;
}
