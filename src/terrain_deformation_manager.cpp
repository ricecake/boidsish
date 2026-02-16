#include "terrain_deformation_manager.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Boidsish {

	TerrainDeformationManager::TerrainDeformationManager(double voxel_size):
		voxel_size_(voxel_size), voxel_grid_(voxel_size) {}

	// ==================== Deformation Management ====================

	uint32_t TerrainDeformationManager::AddDeformation(std::shared_ptr<TerrainDeformation> deformation) {
		std::unique_lock lock(mutex_);

		uint32_t id = deformation->GetId();
		deformations_[id] = deformation;

		// Populate voxel grid with precomputed values
		PopulateVoxelsForDeformation(deformation);

		return id;
	}

	bool TerrainDeformationManager::RemoveDeformation(uint32_t deformation_id) {
		std::unique_lock lock(mutex_);

		auto it = deformations_.find(deformation_id);
		if (it == deformations_.end()) {
			return false;
		}

		ClearVoxelsForDeformation(deformation_id);
		deformations_.erase(it);
		return true;
	}

	int TerrainDeformationManager::RemoveDeformationsInRadius(const glm::vec3& center, float radius) {
		std::unique_lock lock(mutex_);

		std::vector<uint32_t> to_remove;
		float                 radius_sq = radius * radius;

		for (const auto& [id, deformation] : deformations_) {
			glm::vec3 def_center = deformation->GetCenter();
			glm::vec3 diff = def_center - center;
			if (glm::dot(diff, diff) <= radius_sq) {
				to_remove.push_back(id);
			}
		}

		for (uint32_t id : to_remove) {
			ClearVoxelsForDeformation(id);
			deformations_.erase(id);
		}

		return static_cast<int>(to_remove.size());
	}

	void TerrainDeformationManager::Clear() {
		std::unique_lock lock(mutex_);
		voxel_grid_.clear(Bonxai::CLEAR_MEMORY);
		deformations_.clear();
	}

	void TerrainDeformationManager::RefreshDeformationCache(uint32_t deformation_id) {
		std::unique_lock lock(mutex_);

		auto it = deformations_.find(deformation_id);
		if (it == deformations_.end()) {
			return;
		}

		ClearVoxelsForDeformation(deformation_id);
		PopulateVoxelsForDeformation(it->second);
	}

	// ==================== Terrain Query Operations ====================

	DeformationQueryResult TerrainDeformationManager::QueryDeformations(
		float            x,
		float            z,
		float            current_height,
		const glm::vec3& current_normal
	) const {
		std::shared_lock lock(mutex_);

		DeformationQueryResult result;
		result.transformed_normal = current_normal;

		// Collect unique deformations affecting this point
		std::set<uint32_t> affecting_ids;

		// Check the voxel at this position
		Bonxai::CoordT coord = PosToCoord(x, z);
		auto           accessor = voxel_grid_.createConstAccessor();
		const auto*    entry = accessor.value(coord);

		if (entry) {
			for (uint32_t id : entry->deformation_ids) {
				if (id > 0)
					affecting_ids.insert(id);
			}
		}

		// Also check surrounding voxels for deformations that might overlap
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				if (dx == 0 && dz == 0)
					continue;
				Bonxai::CoordT neighbor{coord.x + dx, coord.y, coord.z + dz};
				const auto*    neighbor_entry = accessor.value(neighbor);
				if (neighbor_entry) {
					for (uint32_t id : neighbor_entry->deformation_ids) {
						if (id > 0)
							affecting_ids.insert(id);
					}
				}
			}
		}

		if (affecting_ids.empty()) {
			return result;
		}

		// Compute combined deformation from all affecting deformations
		float     total_weight = 0.0f;
		glm::vec3 weighted_normal{0.0f};

		for (uint32_t id : affecting_ids) {
			auto it = deformations_.find(id);
			if (it == deformations_.end())
				continue;

			const auto& deformation = it->second;
			if (!deformation->ContainsPointXZ(x, z))
				continue;

			DeformationResult def_result = deformation->ComputeDeformation(x, z, current_height, current_normal);

			if (def_result.applies) {
				result.has_deformation = true;
				result.affecting_deformations.push_back(deformation);

				// Accumulate hole status
				if (def_result.is_hole) {
					result.has_hole = true;
				}

				// Accumulate height delta (weighted blend)
				result.total_height_delta += def_result.height_delta * def_result.blend_weight;

				// Accumulate normals for weighted average
				glm::vec3 transformed = deformation->TransformNormal(x, z, current_normal);
				weighted_normal += transformed * def_result.blend_weight;
				total_weight += def_result.blend_weight;
			}
		}

		// Finalize weighted normal
		if (total_weight > 0.0f) {
			weighted_normal /= total_weight;
			result.transformed_normal = glm::normalize(weighted_normal);
		}

		return result;
	}

	bool TerrainDeformationManager::HasDeformationAt(float x, float z) const {
		std::shared_lock lock(mutex_);

		Bonxai::CoordT coord = PosToCoord(x, z);
		auto           accessor = voxel_grid_.createConstAccessor();
		const auto*    entry = accessor.value(coord);

		if (!entry)
			return false;
		for (uint32_t id : entry->deformation_ids) {
			if (id > 0)
				return true;
		}
		return false;
	}

	float TerrainDeformationManager::GetCachedHeightDelta(float x, float z) const {
		std::shared_lock lock(mutex_);

		Bonxai::CoordT coord = PosToCoord(x, z);
		auto           accessor = voxel_grid_.createConstAccessor();
		const auto*    entry = accessor.value(coord);

		if (entry) {
			return entry->precomputed_height_delta;
		}
		return 0.0f;
	}

	std::vector<std::shared_ptr<TerrainDeformation>>
	TerrainDeformationManager::GetDeformationsAt(float x, float z) const {
		std::shared_lock lock(mutex_);

		std::vector<std::shared_ptr<TerrainDeformation>> result;
		std::set<uint32_t>                               found_ids;

		Bonxai::CoordT coord = PosToCoord(x, z);
		auto           accessor = voxel_grid_.createConstAccessor();

		// Check center and neighbors
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				Bonxai::CoordT check{coord.x + dx, coord.y, coord.z + dz};
				const auto*    entry = accessor.value(check);
				if (entry) {
					for (uint32_t id : entry->deformation_ids) {
						if (id > 0 && found_ids.find(id) == found_ids.end()) {
							auto it = deformations_.find(id);
							if (it != deformations_.end() && it->second->ContainsPointXZ(x, z)) {
								result.push_back(it->second);
								found_ids.insert(id);
							}
						}
					}
				}
			}
		}

		return result;
	}

	std::shared_ptr<TerrainDeformation> TerrainDeformationManager::GetDeformation(uint32_t deformation_id) const {
		std::shared_lock lock(mutex_);

		auto it = deformations_.find(deformation_id);
		if (it != deformations_.end()) {
			return it->second;
		}
		return nullptr;
	}

	// ==================== Spatial Queries ====================

	std::vector<std::shared_ptr<TerrainDeformation>>
	TerrainDeformationManager::FindDeformationsInRadius(const glm::vec3& center, float radius) const {
		std::shared_lock lock(mutex_);

		std::vector<std::shared_ptr<TerrainDeformation>> result;
		float                                            radius_sq = radius * radius;

		for (const auto& [id, deformation] : deformations_) {
			glm::vec3 def_center = deformation->GetCenter();
			glm::vec3 diff = def_center - center;

			// Check if deformation's bounding sphere overlaps search sphere
			float def_radius = deformation->GetMaxRadius();
			float combined_radius = radius + def_radius;

			if (glm::dot(diff, diff) <= combined_radius * combined_radius) {
				result.push_back(deformation);
			}
		}

		return result;
	}

	std::vector<std::shared_ptr<TerrainDeformation>>
	TerrainDeformationManager::FindDeformationsInAABB(const glm::vec3& min_corner, const glm::vec3& max_corner) const {
		std::shared_lock lock(mutex_);

		std::vector<std::shared_ptr<TerrainDeformation>> result;

		for (const auto& [id, deformation] : deformations_) {
			glm::vec3 def_min, def_max;
			deformation->GetBounds(def_min, def_max);

			// AABB overlap test
			if (def_max.x >= min_corner.x && def_min.x <= max_corner.x && def_max.y >= min_corner.y &&
			    def_min.y <= max_corner.y && def_max.z >= min_corner.z && def_min.z <= max_corner.z) {
				result.push_back(deformation);
			}
		}

		return result;
	}

	bool TerrainDeformationManager::ChunkHasDeformations(
		float chunk_min_x,
		float chunk_min_z,
		float chunk_max_x,
		float chunk_max_z
	) const {
		std::shared_lock lock(mutex_);

		for (const auto& [id, deformation] : deformations_) {
			glm::vec3 def_min, def_max;
			deformation->GetBounds(def_min, def_max);

			// XZ overlap test
			if (def_max.x >= chunk_min_x && def_min.x <= chunk_max_x && def_max.z >= chunk_min_z &&
			    def_min.z <= chunk_max_z) {
				return true;
			}
		}

		return false;
	}

	// ==================== Utility ====================

	size_t TerrainDeformationManager::GetDeformationCount() const {
		std::shared_lock lock(mutex_);
		return deformations_.size();
	}

	size_t TerrainDeformationManager::GetMemoryUsage() const {
		std::shared_lock lock(mutex_);
		return voxel_grid_.memUsage() + deformations_.size() * sizeof(std::shared_ptr<TerrainDeformation>);
	}

	std::vector<DeformationDescriptor> TerrainDeformationManager::GetAllDescriptors() const {
		std::shared_lock lock(mutex_);

		std::vector<DeformationDescriptor> descriptors;
		descriptors.reserve(deformations_.size());

		for (const auto& [id, deformation] : deformations_) {
			descriptors.push_back(deformation->GetDescriptor());
		}

		return descriptors;
	}

	// ==================== Private Helpers ====================

	void
	TerrainDeformationManager::PopulateVoxelsForDeformation(const std::shared_ptr<TerrainDeformation>& deformation) {
		glm::vec3 min_bound, max_bound;
		deformation->GetBounds(min_bound, max_bound);

		auto accessor = voxel_grid_.createAccessor();

		// Iterate over the XZ footprint of the deformation
		for (float x = min_bound.x; x <= max_bound.x; x += static_cast<float>(voxel_size_)) {
			for (float z = min_bound.z; z <= max_bound.z; z += static_cast<float>(voxel_size_)) {
				if (!deformation->ContainsPointXZ(x, z)) {
					continue;
				}

				// Compute precomputed values for this voxel
				// Use a reference height of 0 for precomputation
				float height_delta = deformation->ComputeHeightDelta(x, z, 0.0f);

				Bonxai::CoordT coord = voxel_grid_.posToCoord(x, 0.0, z);
				auto*          entry = accessor.value(coord);
				if (entry) {
					entry->AddDeformation(deformation->GetId(), height_delta);
				} else {
					DeformationVoxelEntry new_entry(deformation->GetId(), height_delta);
					accessor.setValue(coord, new_entry);
				}
			}
		}
	}

	void TerrainDeformationManager::ClearVoxelsForDeformation(uint32_t deformation_id) {
		auto it = deformations_.find(deformation_id);
		if (it == deformations_.end()) {
			return;
		}

		const auto& deformation = it->second;
		glm::vec3   min_bound, max_bound;
		deformation->GetBounds(min_bound, max_bound);

		auto accessor = voxel_grid_.createAccessor();

		// Clear voxels that belong to this deformation
		for (float x = min_bound.x; x <= max_bound.x; x += static_cast<float>(voxel_size_)) {
			for (float z = min_bound.z; z <= max_bound.z; z += static_cast<float>(voxel_size_)) {
				Bonxai::CoordT coord = voxel_grid_.posToCoord(x, 0.0, z);
				auto*          entry = accessor.value(coord);
				if (entry) {
					float height_delta = deformation->ComputeHeightDelta(x, z, 0.0f);
					entry->RemoveDeformation(deformation_id, height_delta);
					if (entry->IsEmpty()) {
						accessor.setCellOff(coord);
					}
				}
			}
		}
	}

	Bonxai::CoordT TerrainDeformationManager::PosToCoord(float x, float z) const {
		return voxel_grid_.posToCoord(x, 0.0, z);
	}

} // namespace Boidsish
