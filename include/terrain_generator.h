#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "PerlinNoise.hpp"
#include "Simplex.h"
#include "graphics.h"
#include "terrain.h"
#include "thread_pool.h"

namespace Boidsish {

	struct TerrainGenerationResult {
		std::vector<float>        vertex_data;
		std::vector<unsigned int> indices;
		int                       chunk_x;
		int                       chunk_z;
		bool                      has_terrain;
	};

	class TerrainGenerator {
	public:
		TerrainGenerator(int seed = 12345);
		~TerrainGenerator();

		void                                  update(const Frustum& frustum, const Camera& camera);
		std::vector<std::shared_ptr<Terrain>> getVisibleChunks();

		std::tuple<float, glm::vec3> pointProperties(float x, float z) {
			auto point = pointGenerate(x, z);
			auto norm = diffToNorm(point[1], point[2]);
			return std::tuple<float, glm::vec3>(point[0], norm);
		}

	private:
		TerrainGenerationResult generateChunkData(int chunkX, int chunkZ);

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

		const int view_distance_ = 10;        // in chunks
		const int kUnloadDistanceBuffer_ = 2; // in chunks
		const int chunk_size_ = 32;
		int       octaves_ = 4;
		float     lacunarity_ = 0.99f;
		float     persistence_ = 0.5f;

		// Control noise parameters
		constexpr static const float control_noise_scale_ = 0.01f;

		// Noise generators
		siv::PerlinNoise control_perlin_noise_;

		auto      fbm(float x, float z, TerrainParameters params);
		auto      biomefbm(glm::vec2 pos, BiomeAttributes attr);
		glm::vec3 pointGenerate(float x, float y);

		glm::vec3 diffToNorm(float dx, float dz) { return glm::normalize(glm::vec3(-dx, 1.0f, -dz)); }

		// Cache and async management
		ThreadPool                                                         thread_pool_;
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>>            chunk_cache_;
		std::map<std::pair<int, int>, TaskHandle<TerrainGenerationResult>> pending_chunks_;
	};

} // namespace Boidsish
