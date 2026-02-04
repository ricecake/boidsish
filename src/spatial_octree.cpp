#include "spatial_octree.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace Boidsish {

SpatialOctree::SpatialOctree(double voxel_size): voxel_size_(voxel_size), grid_(voxel_size) {}

// ==================== Write Operations ====================

void SpatialOctree::Insert(int entity_id, const glm::vec3& position) {
	std::unique_lock lock(mutex_);

	// Remove from old position if exists
	auto it = entity_coords_.find(entity_id);
	if (it != entity_coords_.end()) {
		auto accessor = grid_.createAccessor();
		accessor.setCellOff(it->second);
	}

	// Insert at new position
	Bonxai::CoordT coord = grid_.posToCoord(position.x, position.y, position.z);
	auto           accessor = grid_.createAccessor();
	accessor.setValue(coord, SpatialEntry(entity_id, position));
	entity_coords_[entity_id] = coord;
}

void SpatialOctree::Insert(int entity_id, const Vector3& position) {
	Insert(entity_id, glm::vec3(position.x, position.y, position.z));
}

bool SpatialOctree::Remove(int entity_id, const glm::vec3& last_known_position) {
	std::unique_lock lock(mutex_);

	auto it = entity_coords_.find(entity_id);
	if (it != entity_coords_.end()) {
		auto accessor = grid_.createAccessor();
		accessor.setCellOff(it->second);
		entity_coords_.erase(it);
		return true;
	}

	// Fallback: try the hint position
	Bonxai::CoordT coord = grid_.posToCoord(last_known_position.x, last_known_position.y, last_known_position.z);
	auto           accessor = grid_.createAccessor();
	auto*          entry = accessor.value(coord);
	if (entry && entry->entity_id == entity_id) {
		accessor.setCellOff(coord);
		return true;
	}

	return false;
}

bool SpatialOctree::Remove(int entity_id) {
	std::unique_lock lock(mutex_);

	auto it = entity_coords_.find(entity_id);
	if (it != entity_coords_.end()) {
		auto accessor = grid_.createAccessor();
		accessor.setCellOff(it->second);
		entity_coords_.erase(it);
		return true;
	}
	return false;
}

void SpatialOctree::Clear() {
	std::unique_lock lock(mutex_);
	grid_.clear(Bonxai::CLEAR_MEMORY);
	entity_coords_.clear();
}

void SpatialOctree::Rebuild(const std::vector<SpatialEntry>& entries) {
	std::unique_lock lock(mutex_);

	grid_.clear(Bonxai::CLEAR_MEMORY);
	entity_coords_.clear();

	auto accessor = grid_.createAccessor();
	for (const auto& entry : entries) {
		Bonxai::CoordT coord = grid_.posToCoord(entry.position.x, entry.position.y, entry.position.z);
		accessor.setValue(coord, entry);
		entity_coords_[entry.entity_id] = coord;
	}
}

// ==================== Read Operations ====================

std::vector<int> SpatialOctree::RadiusSearch(const glm::vec3& center, float radius) const {
	std::vector<int> result;
	std::shared_lock lock(mutex_);

	float radius_sq = radius * radius;

	// Compute AABB for the search sphere
	glm::vec3 min_corner = center - glm::vec3(radius);
	glm::vec3 max_corner = center + glm::vec3(radius);

	ForEachVoxelInAABB(min_corner, max_corner, [&](const SpatialEntry& entry, const Bonxai::CoordT&) {
		glm::vec3 diff = entry.position - center;
		float     dist_sq = glm::dot(diff, diff);
		if (dist_sq <= radius_sq) {
			result.push_back(entry.entity_id);
		}
	});

	return result;
}

std::vector<int> SpatialOctree::RadiusSearch(const Vector3& center, float radius) const {
	return RadiusSearch(glm::vec3(center.x, center.y, center.z), radius);
}

std::vector<SpatialEntry> SpatialOctree::RadiusSearchWithPositions(const glm::vec3& center, float radius) const {
	std::vector<SpatialEntry> result;
	std::shared_lock          lock(mutex_);

	float radius_sq = radius * radius;

	glm::vec3 min_corner = center - glm::vec3(radius);
	glm::vec3 max_corner = center + glm::vec3(radius);

	ForEachVoxelInAABB(min_corner, max_corner, [&](const SpatialEntry& entry, const Bonxai::CoordT&) {
		glm::vec3 diff = entry.position - center;
		float     dist_sq = glm::dot(diff, diff);
		if (dist_sq <= radius_sq) {
			result.push_back(entry);
		}
	});

	return result;
}

int SpatialOctree::NearestNeighbor(const glm::vec3& center, float max_radius) const {
	std::shared_lock lock(mutex_);

	int   nearest_id = -1;
	float nearest_dist_sq = max_radius * max_radius;

	// Use expanding search strategy
	float search_radius = std::min(static_cast<float>(voxel_size_) * 4.0f, max_radius);

	while (search_radius <= max_radius) {
		glm::vec3 min_corner = center - glm::vec3(search_radius);
		glm::vec3 max_corner = center + glm::vec3(search_radius);

		ForEachVoxelInAABB(min_corner, max_corner, [&](const SpatialEntry& entry, const Bonxai::CoordT&) {
			glm::vec3 diff = entry.position - center;
			float     dist_sq = glm::dot(diff, diff);
			if (dist_sq < nearest_dist_sq) {
				nearest_dist_sq = dist_sq;
				nearest_id = entry.entity_id;
			}
		});

		// If we found something within this radius, we're done
		// (because anything outside this radius would be farther)
		if (nearest_id >= 0 && nearest_dist_sq <= search_radius * search_radius) {
			break;
		}

		search_radius *= 2.0f;
	}

	return nearest_id;
}

int SpatialOctree::NearestNeighbor(const Vector3& center, float max_radius) const {
	return NearestNeighbor(glm::vec3(center.x, center.y, center.z), max_radius);
}

std::vector<int> SpatialOctree::KNearestNeighbors(const glm::vec3& center, int k, float max_radius) const {
	std::shared_lock lock(mutex_);

	// Use a max-heap to track k nearest (top = farthest of the k)
	using DistIdPair = std::pair<float, int>;
	std::priority_queue<DistIdPair> max_heap;

	float radius_sq = max_radius * max_radius;

	glm::vec3 min_corner = center - glm::vec3(max_radius);
	glm::vec3 max_corner = center + glm::vec3(max_radius);

	ForEachVoxelInAABB(min_corner, max_corner, [&](const SpatialEntry& entry, const Bonxai::CoordT&) {
		glm::vec3 diff = entry.position - center;
		float     dist_sq = glm::dot(diff, diff);

		if (dist_sq > radius_sq) {
			return;
		}

		if (static_cast<int>(max_heap.size()) < k) {
			max_heap.push({dist_sq, entry.entity_id});
		} else if (dist_sq < max_heap.top().first) {
			max_heap.pop();
			max_heap.push({dist_sq, entry.entity_id});
		}
	});

	// Extract results in order (closest first)
	std::vector<int> result;
	result.reserve(max_heap.size());
	while (!max_heap.empty()) {
		result.push_back(max_heap.top().second);
		max_heap.pop();
	}
	std::reverse(result.begin(), result.end());

	return result;
}

std::vector<int> SpatialOctree::AABBSearch(const glm::vec3& min_corner, const glm::vec3& max_corner) const {
	std::vector<int> result;
	std::shared_lock lock(mutex_);

	ForEachVoxelInAABB(min_corner, max_corner, [&](const SpatialEntry& entry, const Bonxai::CoordT&) {
		// Check exact position is within AABB
		if (entry.position.x >= min_corner.x && entry.position.x <= max_corner.x &&
		    entry.position.y >= min_corner.y && entry.position.y <= max_corner.y &&
		    entry.position.z >= min_corner.z && entry.position.z <= max_corner.z) {
			result.push_back(entry.entity_id);
		}
	});

	return result;
}

std::vector<int> SpatialOctree::AABBSearch(const Vector3& min_corner, const Vector3& max_corner) const {
	return AABBSearch(
		glm::vec3(min_corner.x, min_corner.y, min_corner.z), glm::vec3(max_corner.x, max_corner.y, max_corner.z)
	);
}

std::vector<int> SpatialOctree::OBBSearch(const OrientedBoundingBox& obb) const {
	std::vector<int> result;
	std::shared_lock lock(mutex_);

	// Get AABB that encloses the OBB for broad-phase
	glm::vec3 aabb_min, aabb_max;
	obb.GetEnclosingAABB(aabb_min, aabb_max);

	ForEachVoxelInAABB(aabb_min, aabb_max, [&](const SpatialEntry& entry, const Bonxai::CoordT&) {
		// Narrow-phase: check if point is inside OBB
		if (obb.Contains(entry.position)) {
			result.push_back(entry.entity_id);
		}
	});

	return result;
}

std::vector<SweepResult>
SpatialOctree::Sweep(const glm::vec3& origin, const glm::vec3& direction, float max_distance, float corridor_radius)
	const {
	std::vector<SweepResult> results;
	std::shared_lock         lock(mutex_);

	// Normalize direction
	glm::vec3 dir = glm::normalize(direction);
	if (glm::length(dir) < 0.001f) {
		return results;
	}

	// Compute AABB that encloses the entire sweep corridor
	glm::vec3 end_point = origin + dir * max_distance;

	glm::vec3 min_corner = glm::min(origin, end_point) - glm::vec3(corridor_radius);
	glm::vec3 max_corner = glm::max(origin, end_point) + glm::vec3(corridor_radius);

	float corridor_radius_sq = corridor_radius * corridor_radius;

	ForEachVoxelInAABB(min_corner, max_corner, [&](const SpatialEntry& entry, const Bonxai::CoordT&) {
		// Compute distance along ray and perpendicular distance
		glm::vec3 to_point = entry.position - origin;
		float     along = glm::dot(to_point, dir);

		// Check if within sweep length
		if (along < 0.0f || along > max_distance) {
			return;
		}

		// Compute perpendicular distance to ray
		glm::vec3 closest_on_ray = origin + dir * along;
		glm::vec3 perp = entry.position - closest_on_ray;
		float     perp_dist_sq = glm::dot(perp, perp);

		if (perp_dist_sq <= corridor_radius_sq) {
			results.push_back({entry.entity_id, entry.position, along});
		}
	});

	// Sort by distance along sweep direction
	std::sort(results.begin(), results.end());

	return results;
}

std::vector<SweepResult>
SpatialOctree::Sweep(const Vector3& origin, const Vector3& direction, float max_distance, float corridor_radius)
	const {
	return Sweep(
		glm::vec3(origin.x, origin.y, origin.z), glm::vec3(direction.x, direction.y, direction.z), max_distance,
		corridor_radius
	);
}

int SpatialOctree::SweepFirst(
	const glm::vec3& origin, const glm::vec3& direction, float max_distance, float corridor_radius
) const {
	auto results = Sweep(origin, direction, max_distance, corridor_radius);
	return results.empty() ? -1 : results.front().entity_id;
}

// ==================== Utility ====================

size_t SpatialOctree::Size() const {
	std::shared_lock lock(mutex_);
	return entity_coords_.size();
}

size_t SpatialOctree::MemoryUsage() const {
	std::shared_lock lock(mutex_);
	return grid_.memUsage() + entity_coords_.size() * (sizeof(int) + sizeof(Bonxai::CoordT));
}

// ==================== Private Helpers ====================

template <typename Func>
void SpatialOctree::ForEachVoxelInAABB(const glm::vec3& min_corner, const glm::vec3& max_corner, Func&& func) const {
	// Convert corners to voxel coordinates
	Bonxai::CoordT min_coord = grid_.posToCoord(min_corner.x, min_corner.y, min_corner.z);
	Bonxai::CoordT max_coord = grid_.posToCoord(max_corner.x, max_corner.y, max_corner.z);

	auto accessor = grid_.createConstAccessor();

	// Iterate all voxels in the range
	for (int32_t x = min_coord.x; x <= max_coord.x; ++x) {
		for (int32_t y = min_coord.y; y <= max_coord.y; ++y) {
			for (int32_t z = min_coord.z; z <= max_coord.z; ++z) {
				Bonxai::CoordT coord{x, y, z};
				const auto*    entry = accessor.value(coord);
				if (entry && entry->entity_id >= 0) {
					func(*entry, coord);
				}
			}
		}
	}
}

glm::vec3 SpatialOctree::CoordToPos(const Bonxai::CoordT& coord) const {
	Bonxai::Point3D p = grid_.coordToPos(coord);
	return glm::vec3(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
}

} // namespace Boidsish
