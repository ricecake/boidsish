#include <cassert>
#include <vector>

#include "terrain_generator.h"

int main() {
	Boidsish::TerrainGenerator terrain_generator;
	std::vector<uint16_t>      pixels = terrain_generator.GenerateSuperChunkTexture(0, 0);
	assert(pixels.size() == 128 * 128 * 4);
	return 0;
}
