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

		const int   view_distance_ = 10; // in chunks
		const int   octaves_ = 8;
		const float lacunarity_ = 2.0f;
		const float persistence_ = 0.5f;
		const int   chunk_size_ = lacunarity_*32;

		const TerrainParameters                coast_params_ = {0.5f, 1.50f, 0.6f};
		const TerrainParameters                plains_params_ = {0.7f, 5.0f, 0.5f};
		const TerrainParameters                hills_params_ = {0.5f, 15.0f, 0.4f};
		const TerrainParameters                mountains_params_ = {0.2f, 50.0f, 0.35f};
		const std::array<TerrainParameters, 4> terrain_set =
			{coast_params_, plains_params_, hills_params_, mountains_params_};
		// pick a random number between 1-N-1 (4 here) to pick random pair

		// Control noise parameters
		constexpr static const float control_noise_scale_ = 0.01f;
		constexpr static const float hills_threshold_ = 0.5f;

		// Noise generators
		siv::PerlinNoise perlin_noise_;
		siv::PerlinNoise control_perlin_noise_;

		auto fbm(float x, float z, TerrainParameters params);

		// Cache
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>> chunk_cache_;
	};

} // namespace Boidsish
