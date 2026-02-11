#pragma once

#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "Simplex.h"
#include "constants.h"
#include "terrain.h"
#include "terrain_deformation_manager.h"
#include "terrain_generator_interface.h"
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

	class TerrainGenerator: public ITerrainGenerator {
	public:
		TerrainGenerator(int seed = 12345);
		~TerrainGenerator() override;

		// ==================== ITerrainGenerator Interface ====================

		void                                         Update(const Frustum& frustum, const Camera& camera) override;
		const std::vector<std::shared_ptr<Terrain>>& GetVisibleChunks() const override;
		std::vector<std::shared_ptr<Terrain>>        GetVisibleChunksCopy() const override;

		// Legacy method names for compatibility (call the interface methods)
		void update(const Frustum& frustum, const Camera& camera) { Update(frustum, camera); }

		const std::vector<std::shared_ptr<Terrain>>& getVisibleChunks() const { return GetVisibleChunks(); }

		std::vector<std::shared_ptr<Terrain>> getVisibleChunksCopy() const { return GetVisibleChunksCopy(); }

		/**
		 * @brief Set the terrain render manager for batched rendering.
		 *
		 * When set, the TerrainGenerator will register/unregister chunks with the
		 * render manager instead of using per-chunk GPU resources.
		 *
		 * @param manager The render manager (can be nullptr to disable batched rendering)
		 */
		void SetRenderManager(std::shared_ptr<TerrainRenderManager> manager) override { render_manager_ = manager; }

		/**
		 * @brief Invalidate a chunk that was evicted from the render manager.
		 *
		 * Called by the render manager when a chunk is LRU-evicted due to GPU
		 * texture array capacity limits. This removes the chunk from our cache
		 * so it will be regenerated when it comes back into view.
		 *
		 * @param chunk_key The (chunk_x, chunk_z) key of the evicted chunk
		 */
		void InvalidateChunk(std::pair<int, int> chunk_key) override {
			// No-op: we want to keep the chunk in our CPU cache even if it's
			// evicted from GPU memory, to avoid expensive re-generation.
			// It will be re-registered with the renderer when next visible.
		}

		/**
		 * @brief Get the render manager.
		 */
		std::shared_ptr<TerrainRenderManager> GetRenderManager() const override { return render_manager_; }

		std::vector<uint16_t> GenerateSuperChunkTexture(int requested_x, int requested_z);
		std::vector<uint16_t> GenerateTextureForArea(int world_x, int world_z, int size);
		void                  ConvertDatToPng(const std::string& dat_filepath, const std::string& png_filepath);

		float GetMaxHeight() const override {
			float max_h = 0.0f;
			for (const auto& biome : biomes) {
				max_h = std::max(max_h, biome.floorLevel);
			}
			return max_h * 0.8f * world_scale_;
		}

		int GetChunkSize() const override { return chunk_size_; }

		void SetWorldScale(float scale) override;

		float GetWorldScale() const override { return world_scale_; }

		uint32_t GetVersion() const override { return terrain_version_; }

		// Interface method - calls pointProperties
		std::tuple<float, glm::vec3> GetPointProperties(float x, float z) const override {
			return pointProperties(x, z);
		}

		std::tuple<float, glm::vec3> pointProperties(float x, float z) const {
			std::lock_guard<std::mutex> lock(point_generation_mutex_);
			// Determine grid cell
			float sx = x / world_scale_;
			float sz = z / world_scale_;

			float tx = sx - floor(sx);
			float tz = sz - floor(sz);

			// Get the 4 corner vertices of the grid cell
			float x0 = floor(sx);
			float x1 = x0 + 1.0f;
			float z0 = floor(sz);
			float z1 = z0 + 1.0f;

			auto v0_raw = pointGenerate(x0 * world_scale_, z0 * world_scale_); // Bottom-left
			auto v1_raw = pointGenerate(x1 * world_scale_, z0 * world_scale_); // Bottom-right
			auto v2_raw = pointGenerate(x1 * world_scale_, z1 * world_scale_); // Top-right
			auto v3_raw = pointGenerate(x0 * world_scale_, z1 * world_scale_); // Top-left

			float h0 = v0_raw.x;
			float h1 = v1_raw.x;
			float h2 = v2_raw.x;
			float h3 = v3_raw.x;

			glm::vec3 n0 = diffToNorm(v0_raw.y, v0_raw.z);
			glm::vec3 n1 = diffToNorm(v1_raw.y, v1_raw.z);
			glm::vec3 n2 = diffToNorm(v2_raw.y, v2_raw.z);
			glm::vec3 n3 = diffToNorm(v3_raw.y, v3_raw.z);

			// Apply deformations to corners before Phong interpolation
			if (deformation_manager_.HasDeformations()) {
				float wx0 = x0 * world_scale_;
				float wx1 = x1 * world_scale_;
				float wz0 = z0 * world_scale_;
				float wz1 = z1 * world_scale_;

				auto res0 = deformation_manager_.QueryDeformations(wx0, wz0, h0, n0);
				if (res0.has_deformation) {
					h0 += res0.total_height_delta;
					n0 = res0.transformed_normal;
				}
				auto res1 = deformation_manager_.QueryDeformations(wx1, wz0, h1, n1);
				if (res1.has_deformation) {
					h1 += res1.total_height_delta;
					n1 = res1.transformed_normal;
				}
				auto res2 = deformation_manager_.QueryDeformations(wx1, wz1, h2, n2);
				if (res2.has_deformation) {
					h2 += res2.total_height_delta;
					n2 = res2.transformed_normal;
				}
				auto res3 = deformation_manager_.QueryDeformations(wx0, wz1, h3, n3);
				if (res3.has_deformation) {
					h3 += res3.total_height_delta;
					n3 = res3.transformed_normal;
				}
			}

			glm::vec3 v0 = {x0 * world_scale_, h0, z0 * world_scale_};
			glm::vec3 v1 = {x1 * world_scale_, h1, z0 * world_scale_};
			glm::vec3 v2 = {x1 * world_scale_, h2, z1 * world_scale_};
			glm::vec3 v3 = {x0 * world_scale_, h3, z1 * world_scale_};

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

			// Apply deformations to match the mesh
			if (deformation_manager_.HasDeformationAt(x, z)) {
				auto def_result = deformation_manager_.QueryDeformations(x, z, final_pos.y, final_norm);
				if (def_result.has_deformation) {
					final_pos.y += def_result.total_height_delta;
					final_norm = def_result.transformed_normal;
				}
			}

			return {final_pos.y, final_norm};
		}

		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, float& out_dist) const override;

		std::vector<glm::vec3> GetPath(glm::vec2 start_pos, int num_points, float step_size) const override;

		float     getBiomeControlValue(float x, float z) const;
		glm::vec2 getDomainWarp(float x, float z) const;

		// ========== Cache-Preferring Terrain Queries ==========
		// These methods attempt to use cached chunk data first, falling back to
		// procedural generation only when the query location is outside cached chunks.
		// Much faster for locations within the visible/cached terrain area.

		/**
		 * @brief Get terrain properties at a point, preferring cached chunk data.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return Tuple of (height, surface_normal). Returns (0, up) if no data available.
		 */
		std::tuple<float, glm::vec3> GetCachedPointProperties(float x, float z) const override;

		/**
		 * @brief Check if a 3D point is below the terrain surface.
		 *
		 * Uses cached chunk data when available for fast queries.
		 *
		 * @param point The 3D world position to check
		 * @return true if point.y is below the terrain height at (point.x, point.z)
		 */
		bool IsPointBelowTerrain(const glm::vec3& point) const override;

		/**
		 * @brief Get the signed distance from a point to the terrain surface.
		 *
		 * Positive = above terrain, Negative = below terrain.
		 * Uses cached chunk data when available.
		 *
		 * @param point The 3D world position
		 * @return Signed vertical distance (point.y - terrain_height)
		 */
		float GetDistanceAboveTerrain(const glm::vec3& point) const override;

		/**
		 * @brief Get distance and direction to the closest terrain point.
		 *
		 * Uses cached chunk data for fast approximate queries. The result is
		 * the vector from the input point to the nearest terrain surface point.
		 *
		 * @param point The 3D world position
		 * @return Tuple of (distance, direction_to_terrain). Direction is normalized.
		 *         If point is below terrain, direction points upward.
		 */
		std::tuple<float, glm::vec3> GetClosestTerrainInfo(const glm::vec3& point) const;

		/**
		 * @brief Raycast against terrain, preferring cached chunk data.
		 *
		 * Faster than full Raycast() when the ray is within cached terrain bounds.
		 *
		 * @param origin Ray origin point
		 * @param direction Ray direction (should be normalized)
		 * @param max_distance Maximum distance to check
		 * @param out_distance Output: distance to hit point (if hit)
		 * @param out_normal Output: surface normal at hit point (if hit)
		 * @return true if terrain was hit within max_distance
		 */
		bool RaycastCached(
			const glm::vec3& origin,
			const glm::vec3& direction,
			float            max_distance,
			float&           out_distance,
			glm::vec3&       out_normal
		) const override;

		/**
		 * @brief Check if a world position is within the currently cached terrain area.
		 *
		 * Useful for determining whether cache-preferring queries will be fast.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return true if the position is within a cached chunk
		 */
		bool IsPositionCached(float x, float z) const override;

		// ==================== Terrain Deformation API ====================

		/**
		 * @brief Get the deformation manager for adding/querying terrain deformations.
		 *
		 * The deformation manager allows you to add craters, flatten areas, and
		 * other terrain modifications that will be applied during chunk generation.
		 *
		 * @return Reference to the deformation manager
		 */
		TerrainDeformationManager& GetDeformationManager() override { return deformation_manager_; }

		const TerrainDeformationManager& GetDeformationManager() const override { return deformation_manager_; }

		/**
		 * @brief Add a crater deformation at the specified position.
		 *
		 * Convenience method that creates and adds a CraterDeformation.
		 *
		 * @param center Center position of the crater
		 * @param radius Radius of the crater
		 * @param depth Depth of the crater (positive value)
		 * @param irregularity Random variation amount (0-1)
		 * @param rim_height Height of the rim around the crater
		 * @return ID of the created deformation
		 */
		uint32_t AddCrater(
			const glm::vec3& center,
			float            radius,
			float            depth,
			float            irregularity = 0.2f,
			float            rim_height = 0.0f
		) override;

		/**
		 * @brief Add a flatten square deformation at the specified position.
		 *
		 * Convenience method that creates and adds a FlattenSquareDeformation.
		 *
		 * @param center Center position (Y is the target height)
		 * @param half_width Half-width in X direction
		 * @param half_depth Half-depth in Z direction
		 * @param blend_distance Edge blending distance
		 * @param rotation_y Rotation around Y axis in radians
		 * @return ID of the created deformation
		 */
		uint32_t AddFlattenSquare(
			const glm::vec3& center,
			float            half_width,
			float            half_depth,
			float            blend_distance = 1.0f,
			float            rotation_y = 0.0f
		) override;

		/**
		 * @brief Add an Akira deformation (hemispherical removal).
		 *
		 * @param center Center position
		 * @param radius Radius of the cut at terrain level
		 * @return ID of created deformation
		 */
		uint32_t AddAkira(
			const glm::vec3& center,
			float            radius
		) override;

		/**
		 * @brief Invalidate chunks affected by deformations.
		 *
		 * Call this after adding/removing deformations to regenerate affected chunks.
		 * If deformation_id is provided, only invalidates chunks affected by that
		 * specific deformation. Otherwise invalidates all chunks that have any
		 * deformation.
		 *
		 * @param deformation_id Optional specific deformation to invalidate for
		 */
		void InvalidateDeformedChunks(std::optional<uint32_t> deformation_id = std::nullopt) override;

	private:
		glm::vec2 findClosestPointOnPath(glm::vec2 sample_pos) const;
		glm::vec3 getPathInfluence(float x, float z) const;

		// Helper for cache-based interpolation
		std::optional<std::tuple<float, glm::vec3>> InterpolateFromCachedChunk(float x, float z) const;

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

		const int view_distance_ = Constants::Class::Terrain::DefaultViewDistance();          // in chunks
		const int kUnloadDistanceBuffer_ = Constants::Class::Terrain::UnloadDistanceBuffer(); // in chunks
		const int chunk_size_ = Constants::Class::Terrain::ChunkSize(); // Keep at 32 for performance
		int       octaves_ = Constants::Class::Terrain::DefaultOctaves();
		float     lacunarity_ = Constants::Class::Terrain::DefaultLacunarity();
		float     persistence_ = Constants::Class::Terrain::DefaultPersistence();
		int       seed_;
		float     world_scale_ = 1.0f;
		uint32_t  terrain_version_ = 0;

		// Control noise parameters
		constexpr static const float control_noise_scale_ = Constants::Class::Terrain::ControlNoiseScale();
		constexpr static const float kPathFrequency = Constants::Class::Terrain::PathFrequency();

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
		mutable std::recursive_mutex chunk_cache_mutex_; // Recursive to allow eviction callback
		mutable std::mutex           visible_chunks_mutex_;
		mutable std::mutex           point_generation_mutex_;
		std::random_device           rd_;
		std::mt19937                 eng_;

		// Instanced terrain render manager (optional, when set uses GPU heightmap lookup)
		std::shared_ptr<TerrainRenderManager> render_manager_;

		// Terrain deformation system
		TerrainDeformationManager deformation_manager_;
	};

} // namespace Boidsish
