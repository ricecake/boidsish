#pragma once

#include <map>
#include <memory>
#include <vector>

#include "PerlinNoise.hpp"
#include "Simplex.h"
#include "graphics.h"
#include "terrain.h"

namespace Boidsish {
	class TerrainGenerator {
	public:
		TerrainGenerator(int seed = 12345);

		void                                  update(const Frustum& frustum, const Camera& camera);
		std::vector<std::shared_ptr<Terrain>> getVisibleChunks();

	private:
		std::shared_ptr<Terrain> generateChunk(int chunkX, int chunkZ);

		// Terrain parameters
		struct TerrainParameters {
			float frequency;
			float amplitude;
			float threshold;
		};

		struct BiomeAttributes {
			float spikeDamping;  // How aggressively to cut off sharp gradients
			float detailMasking; // How much valleys should hide high-frequency noise
			float floorLevel;    // The height at which flattening occurs
		};

		std::array<BiomeAttributes, 6> biomes = {
			BiomeAttributes{1.00, 1.0, -0.10},
			BiomeAttributes{0.80, 0.5, 2.0},
			BiomeAttributes{0.05, 0.9, 1.0},
			BiomeAttributes{0.30, 0.2, 8.00},
			BiomeAttributes{0.10, 0.1, 64.0},
			BiomeAttributes{0.05, 0.5, 128}
		};

		const int view_distance_ = 10; // in chunks
		const int kUnloadDistanceBuffer_ = 2; // in chunks
		const int chunk_size_ = 32;
		// Other solid values:  8, 0.05, 0.09
		int   octaves_ = 4;
		float lacunarity_ = 0.99f;
		float persistence_ = 0.5f;

		// Control noise parameters
		constexpr static const float control_noise_scale_ = 0.01f;

		// Noise generators
		siv::PerlinNoise control_perlin_noise_;

		auto fbm(float x, float z, TerrainParameters params);
		auto biomefbm(glm::vec2 pos, BiomeAttributes attr);

		// Cache
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>> chunk_cache_;
	};

} // namespace Boidsish
