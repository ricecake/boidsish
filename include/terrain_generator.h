#pragma once

#include <map>
#include <memory>
#include <vector>

#include "PerlinNoise.hpp"
#include "graphics.h"
#include "terrain.h"

namespace Boidsish {

	class TerrainGenerator {
	public:
		TerrainGenerator(int seed = 12345);

		void                                  update(const Camera& camera);
		std::vector<std::shared_ptr<Terrain>> getVisibleChunks();

	private:
		std::shared_ptr<Terrain> generateChunk(int chunkX, int chunkZ);
		float fbm(float x, float z);

		// Terrain parameters
		const int   chunk_size_ = 32;
		const float frequency_ = 0.1f;
		const float amplitude_ = 5.0f;
		const float threshold_ = 0.5f;
		const int   view_distance_ = 8; // in chunks
		const int   octaves_ = 4;
		const float lacunarity_ = 2.0f;
		const float persistence_ = 0.5f;

		// Noise generator
		siv::PerlinNoise perlin_noise_;

		// Cache
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>> chunk_cache_;
	};

} // namespace Boidsish
