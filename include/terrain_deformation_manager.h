#pragma once

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "terrain_deformation.h"
#include <bonxai/bonxai.hpp>
#include <glm/glm.hpp>

namespace Boidsish {

	static constexpr int MAX_DEFORMATIONS_PER_VOXEL = 8;

	/**
	 * @brief Entry stored in the deformation voxel grid
	 *
	 * Each voxel stores precomputed deformation data plus a reference
	 * to the deformation object for additional queries.
	 */
	struct DeformationVoxelEntry {
		uint32_t deformation_ids[MAX_DEFORMATIONS_PER_VOXEL] = {0};
		float    precomputed_height_delta = 0.0f; // Cached total height delta for this voxel

		DeformationVoxelEntry() = default;

		DeformationVoxelEntry(uint32_t id, float delta): precomputed_height_delta(delta) { deformation_ids[0] = id; }

		bool AddDeformation(uint32_t id, float delta) {
			for (int i = 0; i < MAX_DEFORMATIONS_PER_VOXEL; ++i) {
				if (deformation_ids[i] == id)
					return false;
			}
			for (int i = 0; i < MAX_DEFORMATIONS_PER_VOXEL; ++i) {
				if (deformation_ids[i] == 0) {
					deformation_ids[i] = id;
					precomputed_height_delta += delta;
					return true;
				}
			}
			return false;
		}

		bool RemoveDeformation(uint32_t id, float delta) {
			for (int i = 0; i < MAX_DEFORMATIONS_PER_VOXEL; ++i) {
				if (deformation_ids[i] == id) {
					deformation_ids[i] = 0;
					precomputed_height_delta -= delta;
					return true;
				}
			}
			return false;
		}

		bool IsEmpty() const {
			for (int i = 0; i < MAX_DEFORMATIONS_PER_VOXEL; ++i) {
				if (deformation_ids[i] != 0)
					return false;
			}
			return true;
		}
	};

	/**
	 * @brief Result of querying deformations at a terrain point
	 */
	struct DeformationQueryResult {
		float     total_height_delta = 0.0f;            // Combined height change from all deformations
		glm::vec3 transformed_normal{0.0f, 1.0f, 0.0f}; // Final normal after all transformations
		std::vector<std::shared_ptr<TerrainDeformation>> affecting_deformations; // Deformations at this point
		bool has_deformation = false; // Whether any deformation affects this point
		bool has_hole = false;        // Whether any deformation marks this as a hole
	};

	/**
	 * @brief Manages terrain deformations using a Bonxai voxel grid for spatial queries
	 *
	 * The manager stores deformation objects and maintains a voxel grid that maps
	 * world positions to precomputed deformation values. This allows O(1) lookups
	 * during terrain generation while preserving access to the full deformation
	 * objects for advanced queries.
	 *
	 * Thread Safety:
	 * - Read operations use shared_lock (multiple concurrent readers)
	 * - Write operations use unique_lock (exclusive access)
	 */
	class TerrainDeformationManager {
	public:
		/**
		 * @brief Construct a deformation manager
		 * @param voxel_size Size of voxels in the spatial grid (smaller = more precision)
		 */
		explicit TerrainDeformationManager(double voxel_size = 0.5);

		~TerrainDeformationManager() = default;

		// Non-copyable, non-movable
		TerrainDeformationManager(const TerrainDeformationManager&) = delete;
		TerrainDeformationManager& operator=(const TerrainDeformationManager&) = delete;
		TerrainDeformationManager(TerrainDeformationManager&&) = delete;
		TerrainDeformationManager& operator=(TerrainDeformationManager&&) = delete;

		// ==================== Deformation Management (Write Operations) ====================

		/**
		 * @brief Add a deformation to the manager
		 *
		 * The deformation's affected voxels are computed and cached in the grid.
		 *
		 * @param deformation The deformation to add (ownership transferred)
		 * @return The deformation's ID
		 */
		uint32_t AddDeformation(std::shared_ptr<TerrainDeformation> deformation);

		/**
		 * @brief Remove a deformation by ID
		 * @param deformation_id ID of the deformation to remove
		 * @return true if deformation was found and removed
		 */
		bool RemoveDeformation(uint32_t deformation_id);

		/**
		 * @brief Remove all deformations within a radius of a point
		 * @param center Center point
		 * @param radius Search radius
		 * @return Number of deformations removed
		 */
		int RemoveDeformationsInRadius(const glm::vec3& center, float radius);

		/**
		 * @brief Clear all deformations
		 */
		void Clear();

		/**
		 * @brief Regenerate the voxel cache for a specific deformation
		 *
		 * Call this if a deformation's parameters have changed (though deformations
		 * are typically immutable).
		 */
		void RefreshDeformationCache(uint32_t deformation_id);

		// ==================== Terrain Query Operations (Read Operations) ====================

		/**
		 * @brief Query the total deformation at a terrain point
		 *
		 * This is the primary method for terrain generation. It returns the
		 * combined effect of all deformations affecting the query point.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @param current_height Current terrain height (before deformation)
		 * @param current_normal Current terrain normal (before deformation)
		 * @return DeformationQueryResult with combined height delta and transformed normal
		 */
		DeformationQueryResult
		QueryDeformations(float x, float z, float current_height, const glm::vec3& current_normal) const;

		/**
		 * @brief Fast check if any deformation affects a point
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return true if at least one deformation affects this point
		 */
		bool HasDeformationAt(float x, float z) const;

		/**
		 * @brief Get precomputed height delta from the voxel cache
		 *
		 * Faster than full QueryDeformations when only height is needed.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return Height delta (0 if no deformation)
		 */
		float GetCachedHeightDelta(float x, float z) const;

		/**
		 * @brief Get all deformations affecting a point
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @return Vector of deformation pointers (may be empty)
		 */
		std::vector<std::shared_ptr<TerrainDeformation>> GetDeformationsAt(float x, float z) const;

		/**
		 * @brief Get a deformation by ID
		 * @param deformation_id The deformation's unique ID
		 * @return Shared pointer to deformation, or nullptr if not found
		 */
		std::shared_ptr<TerrainDeformation> GetDeformation(uint32_t deformation_id) const;

		// ==================== Spatial Queries ====================

		/**
		 * @brief Find all deformations within a radius
		 * @param center Query center
		 * @param radius Search radius
		 * @return Vector of deformation pointers
		 */
		std::vector<std::shared_ptr<TerrainDeformation>>
		FindDeformationsInRadius(const glm::vec3& center, float radius) const;

		/**
		 * @brief Find all deformations within an axis-aligned bounding box
		 * @param min_corner Minimum corner
		 * @param max_corner Maximum corner
		 * @return Vector of deformation pointers
		 */
		std::vector<std::shared_ptr<TerrainDeformation>>
		FindDeformationsInAABB(const glm::vec3& min_corner, const glm::vec3& max_corner) const;

		/**
		 * @brief Check if a chunk has any deformations
		 *
		 * Fast check for terrain generation to skip deformation processing
		 * for chunks with no deformations.
		 *
		 * @param chunk_min_x Minimum X of chunk bounds
		 * @param chunk_min_z Minimum Z of chunk bounds
		 * @param chunk_max_x Maximum X of chunk bounds
		 * @param chunk_max_z Maximum Z of chunk bounds
		 * @return true if any deformation overlaps the chunk
		 */
		bool ChunkHasDeformations(float chunk_min_x, float chunk_min_z, float chunk_max_x, float chunk_max_z) const;

		// ==================== Utility ====================

		/**
		 * @brief Get the number of active deformations
		 */
		size_t GetDeformationCount() const;

		/**
		 * @brief Check if there are any deformations
		 */
		bool HasDeformations() const { return GetDeformationCount() > 0; }

		/**
		 * @brief Get approximate memory usage
		 */
		size_t GetMemoryUsage() const;

		/**
		 * @brief Get the voxel resolution
		 */
		double GetVoxelSize() const { return voxel_size_; }

		/**
		 * @brief Get all deformation descriptors for serialization
		 */
		std::vector<DeformationDescriptor> GetAllDescriptors() const;

	private:
		/**
		 * @brief Populate voxel grid for a deformation's area of effect
		 */
		void PopulateVoxelsForDeformation(const std::shared_ptr<TerrainDeformation>& deformation);

		/**
		 * @brief Clear voxels associated with a deformation
		 */
		void ClearVoxelsForDeformation(uint32_t deformation_id);

		/**
		 * @brief Convert world position to voxel coordinate
		 */
		Bonxai::CoordT PosToCoord(float x, float z) const;

		double                                   voxel_size_;
		Bonxai::VoxelGrid<DeformationVoxelEntry> voxel_grid_;
		mutable std::shared_mutex                mutex_;

		// Deformation storage by ID
		std::unordered_map<uint32_t, std::shared_ptr<TerrainDeformation>> deformations_;
		std::atomic<uint32_t>                                             next_id_{1};
	};

} // namespace Boidsish
