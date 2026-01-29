#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

#include "Simplex.h"
#include "terrain.h"
#include "thread_pool.h"

// #include <FastNoise/FastNoise.h>

namespace Boidsish {
	struct Frustum;
	struct Camera;
} // namespace Boidsish

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
		const std::vector<std::shared_ptr<Terrain>>& getVisibleChunks() const;
		std::vector<std::shared_ptr<Terrain>>        getVisibleChunksCopy() const;

		std::vector<uint16_t> GenerateSuperChunkTexture(int requested_x, int requested_z);
		std::vector<uint16_t> GenerateTextureForArea(int world_x, int world_z, int size);
		void                  ConvertDatToPng(const std::string& dat_filepath, const std::string& png_filepath);

		float GetMaxHeight() const {
			float max_h = 0.0f;
			for (const auto& biome : biomes) {
				max_h = std::max(max_h, biome.floorLevel);
			}
			return max_h * 0.8;
		}

		std::tuple<float, glm::vec3> pointProperties(float x, float z) const;

		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, float& out_dist) const;

		std::vector<glm::vec3> GetPath(glm::vec2 start_pos, int num_points, float step_size) const;

		float     getBiomeControlValue(float x, float z) const;
		glm::vec2 getDomainWarp(float x, float z) const;

	private:
		glm::vec2 findClosestPointOnPath(glm::vec2 sample_pos) const;
		glm::vec3 getPathInfluence(float x, float z) const;
		std::pair<float, glm::vec3> getPointData(float x, float z) const;

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
			float weight = 1.0f; // How much weight to give this Biome
		};

		inline static const std::array<BiomeAttributes, 8> biomes = {
			BiomeAttributes{1.0, 0.9, 5.0, 5.0f},
			BiomeAttributes{0.80, 0.5, 20.0, 3.0f},
			BiomeAttributes{0.05, 0.6, 40.0, 2.0f},
			BiomeAttributes{0.30, 0.5, 60.00, 1.0f},
			BiomeAttributes{0.40, 0.4, 80.00, 6.0f},
			BiomeAttributes{0.30, 0.2, 100.00, 1.0f},
			BiomeAttributes{0.10, 0.1, 150.0, 3.0f},
			BiomeAttributes{0.05, 0.5, 250.0, 5.0f}
		};

		void ApplyWeightedBiome(float control_value, BiomeAttributes& current) const;

		const int view_distance_ = 10;        // in chunks
		const int kUnloadDistanceBuffer_ = 2; // in chunks
		const int chunk_size_ = 32;
		int       octaves_ = 4;
		float     lacunarity_ = 0.99f;
		float     persistence_ = 0.5f;
		int       seed_;

		// Control noise parameters
		constexpr static const float control_noise_scale_ = 0.001f;
		constexpr static const float kPathFrequency = 0.002f;

		// Noise generators
		// FastNoise::SmartNode<> control_noise_generator_;

		auto      fbm(float x, float z, TerrainParameters params);
		auto      biomefbm(glm::vec2 pos, BiomeAttributes attr) const;
		glm::vec3 pointGenerate(float x, float y) const;

		glm::vec3 diffToNorm(float dx, float dz) const { return glm::normalize(glm::vec3(-dx, 1.0f, -dz)); }

		// Cache and async management
		ThreadPool                                                         thread_pool_;
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>>            chunk_cache_;
		std::vector<std::shared_ptr<Terrain>>                              visible_chunks_;
		std::map<std::pair<int, int>, TaskHandle<TerrainGenerationResult>> pending_chunks_;
		mutable std::mutex                                                 chunk_cache_mutex_;
		mutable std::mutex                                                 visible_chunks_mutex_;
		mutable std::recursive_mutex                                       point_generation_mutex_;
		std::random_device                                                 rd_;
		std::mt19937                                                       eng_;
	};

} // namespace Boidsish
