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

		const int chunk_size_ = 32;
		const int view_distance_ = 8; // in chunks
		const int   octaves_ = 4;
		const float lacunarity_ = 2.0f;
		const float persistence_ = 0.5f;

		const TerrainParameters plains_params_ = {0.1f, 5.0f, 0.5f};
		const TerrainParameters hills_params_ = {0.05f, 15.0f, 0.4f};
		const TerrainParameters mountains_params_ = {0.02f, 50.0f, 0.35f};

		// Control noise parameters
		constexpr static const float control_noise_scale_ = 0.01f;
		constexpr static const float hills_threshold_ = 0.5f;

		// Noise generators
		siv::PerlinNoise perlin_noise_;
		siv::PerlinNoise control_perlin_noise_;

		float fbm(float x, float z, TerrainParameters params);

		// Cache
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>> chunk_cache_;
	};

} // namespace Boidsish
