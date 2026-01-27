#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "Simplex.h"
#include "terrain.h"
#include "terrain_render_manager.h"
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

		/**
		 * @brief Set the terrain render manager for batched rendering.
		 *
		 * When set, the TerrainGenerator will register/unregister chunks with the
		 * render manager instead of using per-chunk GPU resources.
		 *
		 * @param manager The render manager (can be nullptr to disable batched rendering)
		 */
		void SetRenderManager(std::shared_ptr<TerrainRenderManager> manager) {
			render_manager_ = manager;
		}

		/**
		 * @brief Invalidate a chunk that was evicted from the render manager.
		 *
		 * Called by the render manager when a chunk is LRU-evicted due to GPU
		 * texture array capacity limits. This removes the chunk from our cache
		 * so it will be regenerated when it comes back into view.
		 *
		 * @param chunk_key The (chunk_x, chunk_z) key of the evicted chunk
		 */
		void InvalidateChunk(std::pair<int, int> chunk_key) {
			std::lock_guard<std::recursive_mutex> lock(chunk_cache_mutex_);
			chunk_cache_.erase(chunk_key);
		}

		/**
		 * @brief Get the render manager.
		 */
		std::shared_ptr<TerrainRenderManager> GetRenderManager() const {
			return render_manager_;
		}

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

		std::tuple<float, glm::vec3> pointProperties(float x, float z) const {
			std::lock_guard<std::mutex> lock(point_generation_mutex_);
			// Determine grid cell
			float tx = x - floor(x);
			float tz = z - floor(z);

			// Get the 4 corner vertices of the grid cell
			auto v0_raw = pointGenerate(floor(x), floor(z)); // Bottom-left
			auto v1_raw = pointGenerate(ceil(x), floor(z));  // Bottom-right
			auto v2_raw = pointGenerate(ceil(x), ceil(z));   // Top-right
			auto v3_raw = pointGenerate(floor(x), ceil(z));  // Top-left

			glm::vec3 v0 = {floor(x), v0_raw.x, floor(z)};
			glm::vec3 v1 = {ceil(x), v1_raw.x, floor(z)};
			glm::vec3 v2 = {ceil(x), v2_raw.x, ceil(z)};
			glm::vec3 v3 = {floor(x), v3_raw.x, ceil(z)};

			glm::vec3 n0 = diffToNorm(v0_raw.y, v0_raw.z);
			glm::vec3 n1 = diffToNorm(v1_raw.y, v1_raw.z);
			glm::vec3 n2 = diffToNorm(v2_raw.y, v2_raw.z);
			glm::vec3 n3 = diffToNorm(v3_raw.y, v3_raw.z);

			// The "flat" position from standard bilinear interpolation
			glm::vec3 q = bilerp(v0, v1, v2, v3, {tx, tz});

			// Phong Tessellation: Project q onto the tangent plane of each corner
			glm::vec3 p0 = projectPointOnPlane(q, v0, n0);
			glm::vec3 p1 = projectPointOnPlane(q, v1, n1);
			glm::vec3 p2 = projectPointOnPlane(q, v2, n2);
			glm::vec3 p3 = projectPointOnPlane(q, v3, n3);

			// Interpolate the projected points to find the final curved position
			glm::vec3 final_pos = bilerp(p0, p1, p2, p3, {tx, tz});

			// Interpolate normals for lighting
			glm::vec3 final_norm = glm::normalize(bilerp(n0, n1, n2, n3, {tx, tz}));

			return {final_pos.y, final_norm};
		}

		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, float& out_dist) const;

		std::vector<glm::vec3> GetPath(glm::vec2 start_pos, int num_points, float step_size) const;

		float     getBiomeControlValue(float x, float z) const;
		glm::vec2 getDomainWarp(float x, float z) const;

	private:
		glm::vec2 findClosestPointOnPath(glm::vec2 sample_pos) const;
		glm::vec3 getPathInfluence(float x, float z) const;

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
	const int chunk_size_ = 32;           // Keep at 32 for performance
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
		mutable std::recursive_mutex                                       chunk_cache_mutex_;  // Recursive to allow eviction callback
		mutable std::mutex                                                 visible_chunks_mutex_;
		mutable std::mutex                                                 point_generation_mutex_;
		std::random_device                                                 rd_;
		std::mt19937                                                       eng_;

		// Instanced terrain render manager (optional, when set uses GPU heightmap lookup)
		std::shared_ptr<TerrainRenderManager> render_manager_;
	};

} // namespace Boidsish
