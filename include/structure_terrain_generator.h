#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <vector>

#include "terrain_generator_interface.h"
#include "terrain.h"
#include "terrain_render_interface.h"
#include "thread_pool.h"

namespace Boidsish {

	/**
	 * @brief Terrain generator that produces an enclosed structure (e.g., a tunnel or corridor).
	 *
	 * This implementation generates mesh data with both a floor and a ceiling,
	 * creating an enclosed space.
	 */
	class StructureTerrainGenerator : public ITerrainGenerator {
	public:
		StructureTerrainGenerator(int seed = 54321);
		~StructureTerrainGenerator() override;

		void Update(const Frustum& frustum, const Camera& camera) override;

		const std::vector<std::shared_ptr<Terrain>>& GetVisibleChunks() const override;
		std::vector<std::shared_ptr<Terrain>>        GetVisibleChunksCopy() const override;

		void SetRenderManager(std::shared_ptr<ITerrainRenderManager> manager) override { render_manager_ = manager; }
		std::shared_ptr<ITerrainRenderManager> GetRenderManager() const override { return render_manager_; }

		void InvalidateChunk(std::pair<int, int> chunk_key) override {}

		std::tuple<float, glm::vec3> CalculateTerrainPropertiesAtPoint(float x, float z) const override;
		std::tuple<float, glm::vec3> GetTerrainPropertiesAtPoint(float x, float z) const override;

		bool IsPointBelowTerrain(const glm::vec3& point) const override;
		float GetDistanceAboveTerrain(const glm::vec3& point) const override;
		bool IsPositionCached(float x, float z) const override;

		bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, float& out_distance) const override;
		bool RaycastCached(
			const glm::vec3& origin,
			const glm::vec3& direction,
			float            max_distance,
			float&           out_distance,
			glm::vec3&       out_normal
		) const override;

		TerrainDeformationManager&       GetDeformationManager() override { return deformation_manager_; }
		const TerrainDeformationManager& GetDeformationManager() const override { return deformation_manager_; }

		uint32_t AddCrater(const glm::vec3& center, float radius, float depth, float irregularity = 0.2f, float rim_height = 0.0f) override { return 0; }
		uint32_t AddFlattenSquare(const glm::vec3& center, float half_width, float half_depth, float blend_distance = 1.0f, float rotation_y = 0.0f) override { return 0; }
		uint32_t AddAkira(const glm::vec3& center, float radius) override { return 0; }
		void InvalidateDeformedChunks(std::optional<uint32_t> deformation_id = std::nullopt) override {}

		float GetMaxHeight() const override { return 50.0f * world_scale_; }
		int GetChunkSize() const override { return 32; }

		void SetWorldScale(float scale) override { world_scale_ = scale; }
		float GetWorldScale() const override { return world_scale_; }

		uint32_t GetVersion() const override { return 1; }

		std::vector<glm::vec3> GetPath(glm::vec2 start_pos, int num_points, float step_size) const override { return {}; }
		glm::vec3              GetPathData(float x, float z) const override { return glm::vec3(0.0f); }

	private:
		struct StructureGenerationResult {
			std::vector<unsigned int> indices;
			std::vector<glm::vec3>    positions;
			std::vector<glm::vec3>    normals;
			std::vector<glm::vec2>    biomes;
			PatchProxy                proxy;
			int                       chunk_x;
			int                       chunk_z;
		};

		StructureGenerationResult _GenerateChunkData(int chunkX, int chunkZ);

		int seed_;
		float world_scale_ = 1.0f;
		int view_distance_ = 8;

		ThreadPool thread_pool_;
		std::map<std::pair<int, int>, std::shared_ptr<Terrain>> chunk_cache_;
		std::vector<std::shared_ptr<Terrain>> visible_chunks_;
		std::map<std::pair<int, int>, TaskHandle<StructureGenerationResult>> pending_chunks_;

		mutable std::mutex mutex_;
		std::shared_ptr<ITerrainRenderManager> render_manager_;
		TerrainDeformationManager deformation_manager_;
	};

} // namespace Boidsish
