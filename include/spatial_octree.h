#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <shared_mutex>
#include <vector>

#include <bonxai/bonxai.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vector.h"

namespace Boidsish {

/**
 * @brief Entry stored in the spatial octree
 *
 * Contains entity ID and position for spatial queries.
 */
struct SpatialEntry {
	int       entity_id = -1;
	glm::vec3 position{0.0f};

	SpatialEntry() = default;
	SpatialEntry(int id, const glm::vec3& pos): entity_id(id), position(pos) {}
};

/**
 * @brief Result from a sweep query along a vector
 */
struct SweepResult {
	int       entity_id = -1;
	glm::vec3 position{0.0f};
	float     distance = 0.0f; // Distance along sweep vector from origin

	bool operator<(const SweepResult& other) const { return distance < other.distance; }
};

/**
 * @brief Oriented Bounding Box for non-axis-aligned queries
 */
struct OrientedBoundingBox {
	glm::vec3 center{0.0f};
	glm::vec3 half_extents{0.0f}; // Half-size along each local axis
	glm::mat3 orientation{1.0f};  // Rotation matrix (columns are local axes)

	OrientedBoundingBox() = default;
	OrientedBoundingBox(const glm::vec3& c, const glm::vec3& extents, const glm::mat3& rot):
		center(c), half_extents(extents), orientation(rot) {}

	/**
	 * @brief Test if a point is inside the OBB
	 */
	bool Contains(const glm::vec3& point) const {
		// Transform point to local OBB space
		glm::vec3 local = glm::transpose(orientation) * (point - center);
		return std::abs(local.x) <= half_extents.x && std::abs(local.y) <= half_extents.y &&
		       std::abs(local.z) <= half_extents.z;
	}

	/**
	 * @brief Get the AABB that fully contains this OBB (for broad-phase queries)
	 */
	void GetEnclosingAABB(glm::vec3& out_min, glm::vec3& out_max) const {
		// Compute the extent in world space by projecting OBB axes
		glm::vec3 extent{0.0f};
		for (int i = 0; i < 3; ++i) {
			glm::vec3 axis = orientation[i] * half_extents[i];
			extent.x += std::abs(axis.x);
			extent.y += std::abs(axis.y);
			extent.z += std::abs(axis.z);
		}
		out_min = center - extent;
		out_max = center + extent;
	}
};

/**
 * @brief Thread-safe spatial octree wrapper using Bonxai VoxelGrid
 *
 * Provides efficient spatial queries with proper locking semantics:
 * - Multiple readers can access simultaneously (shared_lock)
 * - Writers have exclusive access (unique_lock)
 *
 * The octree stores entity IDs at voxel locations, enabling fast
 * spatial queries like radius search, nearest neighbor, and ray sweeps.
 */
class SpatialOctree {
public:
	/**
	 * @brief Construct a spatial octree
	 * @param voxel_size Size of each voxel cell (smaller = more precision, more memory)
	 */
	explicit SpatialOctree(double voxel_size = 1.0);

	~SpatialOctree() = default;

	// Non-copyable, non-movable (due to mutex)
	SpatialOctree(const SpatialOctree&) = delete;
	SpatialOctree& operator=(const SpatialOctree&) = delete;
	SpatialOctree(SpatialOctree&&) = delete;
	SpatialOctree& operator=(SpatialOctree&&) = delete;

	// ==================== Write Operations (exclusive lock) ====================

	/**
	 * @brief Insert or update an entity's position
	 * @param entity_id Unique entity identifier
	 * @param position World-space position
	 */
	void Insert(int entity_id, const glm::vec3& position);
	void Insert(int entity_id, const Vector3& position);

	/**
	 * @brief Remove an entity from the octree
	 * @param entity_id Entity to remove
	 * @param last_known_position Hint for faster removal (optional)
	 * @return true if entity was found and removed
	 */
	bool Remove(int entity_id, const glm::vec3& last_known_position);
	bool Remove(int entity_id);

	/**
	 * @brief Clear all entries from the octree
	 */
	void Clear();

	/**
	 * @brief Rebuild the octree from a list of entries
	 *
	 * More efficient than individual inserts when updating many entities.
	 */
	void Rebuild(const std::vector<SpatialEntry>& entries);

	// ==================== Read Operations (shared lock) ====================

	/**
	 * @brief Find all entities within a radius of a point
	 * @param center Query center point
	 * @param radius Search radius
	 * @return Vector of entity IDs within the radius
	 */
	std::vector<int> RadiusSearch(const glm::vec3& center, float radius) const;
	std::vector<int> RadiusSearch(const Vector3& center, float radius) const;

	/**
	 * @brief Find all entities within a radius, with position data
	 * @param center Query center point
	 * @param radius Search radius
	 * @return Vector of SpatialEntry with ID and position
	 */
	std::vector<SpatialEntry> RadiusSearchWithPositions(const glm::vec3& center, float radius) const;

	/**
	 * @brief Find the nearest entity to a point
	 * @param center Query point
	 * @param max_radius Maximum search radius (for efficiency)
	 * @return Entity ID of nearest entity, or -1 if none found
	 */
	int NearestNeighbor(const glm::vec3& center, float max_radius = std::numeric_limits<float>::max()) const;
	int NearestNeighbor(const Vector3& center, float max_radius = std::numeric_limits<float>::max()) const;

	/**
	 * @brief Find K nearest neighbors to a point
	 * @param center Query point
	 * @param k Number of neighbors to find
	 * @param max_radius Maximum search radius
	 * @return Vector of entity IDs sorted by distance (closest first)
	 */
	std::vector<int> KNearestNeighbors(const glm::vec3& center, int k, float max_radius) const;

	/**
	 * @brief Find all entities within an axis-aligned bounding box
	 * @param min_corner Minimum corner of AABB
	 * @param max_corner Maximum corner of AABB
	 * @return Vector of entity IDs within the box
	 */
	std::vector<int> AABBSearch(const glm::vec3& min_corner, const glm::vec3& max_corner) const;
	std::vector<int> AABBSearch(const Vector3& min_corner, const Vector3& max_corner) const;

	/**
	 * @brief Find all entities within an oriented bounding box
	 * @param obb The oriented bounding box
	 * @return Vector of entity IDs within the OBB
	 */
	std::vector<int> OBBSearch(const OrientedBoundingBox& obb) const;

	/**
	 * @brief Sweep along a vector and return entities in order of encounter
	 *
	 * Useful for sorting entities by spatial location along a direction,
	 * or for simple ray-casting style queries.
	 *
	 * @param origin Start point of the sweep
	 * @param direction Direction to sweep (will be normalized)
	 * @param max_distance Maximum distance to sweep
	 * @param corridor_radius Radius around the ray to include entities
	 * @return Vector of SweepResult sorted by distance from origin
	 */
	std::vector<SweepResult>
	Sweep(const glm::vec3& origin, const glm::vec3& direction, float max_distance, float corridor_radius) const;

	std::vector<SweepResult>
	Sweep(const Vector3& origin, const Vector3& direction, float max_distance, float corridor_radius) const;

	/**
	 * @brief Get the first entity encountered along a sweep
	 * @param origin Start point
	 * @param direction Sweep direction
	 * @param max_distance Maximum distance
	 * @param corridor_radius Corridor width
	 * @return Entity ID of first hit, or -1 if none
	 */
	int SweepFirst(const glm::vec3& origin, const glm::vec3& direction, float max_distance, float corridor_radius)
		const;

	// ==================== Utility ====================

	/**
	 * @brief Get the number of entities currently stored
	 */
	size_t Size() const;

	/**
	 * @brief Get approximate memory usage in bytes
	 */
	size_t MemoryUsage() const;

	/**
	 * @brief Get the voxel resolution
	 */
	double VoxelSize() const { return voxel_size_; }

private:
	// Internal helper to iterate voxels in an AABB range
	template <typename Func>
	void ForEachVoxelInAABB(const glm::vec3& min_corner, const glm::vec3& max_corner, Func&& func) const;

	// Get position from voxel coordinate
	glm::vec3 CoordToPos(const Bonxai::CoordT& coord) const;

	double                                voxel_size_;
	Bonxai::VoxelGrid<SpatialEntry>       grid_;
	mutable std::shared_mutex             mutex_;
	std::unordered_map<int, Bonxai::CoordT> entity_coords_; // Track entity locations for removal
};

} // namespace Boidsish
