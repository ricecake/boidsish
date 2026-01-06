#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "FastNoise/FastNoise.h"
#include "Simplex.h"

namespace Boidsish {
	struct Frustum;
	struct Camera;
} // namespace Boidsish

#include "terrain.h"
#include "thread_pool.h"

namespace Boidsish {

	struct TerrainGenerationResult {
		std::vector<unsigned int> indices;
		std::vector<glm::vec3>    positions;
		std::vector<glm::vec3>    normals;
		PatchProxy                proxy;
		int                       chunk_x;
		int                       chunk_z;
		bool                      has_terrain;
	};

	class TerrainGenerator {
	public:
		TerrainGenerator(int seed = 12345);
		~TerrainGenerator();

		void                                         update(const Frustum& frustum, const Camera& camera);
		const std::vector<std::shared_ptr<Terrain>>& getVisibleChunks();

		float GetMaxHeight() const {
			float max_h = 0.0f;
			for (const auto& biome : biomes) {
				max_h = std::max(max_h, biome.floorLevel);
			}
			return max_h * 0.8;
		}

		std::tuple<float, glm::vec3> pointProperties(float x, float z) const;
		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, float& out_dist) const;

	private:
		// Phong tessellation helpers (matching the shader)
		glm::vec3 projectPointOnPlane(glm::vec3 q, glm::vec3 v, glm::vec3 n) const {
			return q - glm::dot(q - v, n) * n;
		}

		glm::vec3 bilerp(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec2 uv) const {
			glm::vec3 bot = glm::mix(v0, v1, uv.x);
			glm::vec3 top = glm::mix(v3, v2, uv.x);
			return glm::mix(bot, top, uv.y);
		}

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
        int       seed_;

		// Control noise parameters
		constexpr static const float control_noise_scale_ = 0.01f;

		// Noise generators
        FastNoise::SmartNode<> control_noise_generator_;

		auto      fbm(float x, float z, TerrainParameters params);
		auto      biomefbm(glm::vec2 pos, BiomeAttributes attr) const;
		glm::vec3 pointGenerate(float x, float y) const;

		glm::vec3 diffToNorm(float dx, float dz) const { return glm::normalize(glm::vec3(-dx, 1.0f, -dz)); }

		// Cache and async management
		ThreadPool                                                         thread_pool_;
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>>            chunk_cache_;
		std::vector<std::shared_ptr<Terrain>>                              visible_chunks_;
		std::map<std::pair<int, int>, TaskHandle<TerrainGenerationResult>> pending_chunks_;
	};

} // namespace Boidsish
