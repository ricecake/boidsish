#include "rocket_voxel_tree.h"

#include <algorithm>
#include <cmath>

namespace Boidsish {

	RocketVoxelTree::RocketVoxelTree(double voxel_size): voxel_size_(voxel_size), grid_(voxel_size) {}

	void RocketVoxelTree::AddSegment(const glm::vec3& p1, const glm::vec3& p2, float time) {
		std::unique_lock lock(mutex_);
		auto             accessor = grid_.createAccessor();

		float dist = glm::distance(p1, p2);
		// Sample at half voxel size to ensure no gaps
		int steps = static_cast<int>(std::ceil(dist / (voxel_size_ * 0.5f))) + 1;

		for (int i = 0; i <= steps; ++i) {
			float     t = (steps > 0) ? static_cast<float>(i) / steps : 0.0f;
			glm::vec3 p = glm::mix(p1, p2, t);
			Bonxai::CoordT coord = grid_.posToCoord(p.x, p.y, p.z);
			accessor.setValue(coord, time);
		}
	}

	void RocketVoxelTree::Prune(float currentTime, float maxAge) {
		std::unique_lock lock(mutex_);

		std::vector<Bonxai::CoordT> to_remove;
		grid_.forEachCell([&](float& timestamp, const Bonxai::CoordT& coord) {
			if (currentTime - timestamp > maxAge) {
				to_remove.push_back(coord);
			}
		});

		if (to_remove.empty())
			return;

		auto accessor = grid_.createAccessor();
		for (const auto& coord : to_remove) {
			accessor.setCellOff(coord);
		}

		// Periodically release memory
		static int prune_count = 0;
		if (++prune_count % 100 == 0) {
			grid_.releaseUnusedMemory();
		}
	}

	void RocketVoxelTree::Clear() {
		std::unique_lock lock(mutex_);
		grid_.clear(Bonxai::CLEAR_MEMORY);
	}

	std::vector<RocketVoxel> RocketVoxelTree::GetActiveVoxels() const {
		std::vector<RocketVoxel> result;
		ForEachActiveVoxel([&](const glm::vec3& pos, float timestamp) { result.push_back({pos, timestamp}); });
		return result;
	}

	std::vector<RocketVoxel>
	RocketVoxelTree::GetActiveVoxels(const glm::vec3& minBound, const glm::vec3& maxBound) const {
		std::vector<RocketVoxel> result;
		ForEachActiveVoxel([&](const glm::vec3& pos, float timestamp) {
			if (pos.x >= minBound.x && pos.x <= maxBound.x && pos.y >= minBound.y && pos.y <= maxBound.y &&
			    pos.z >= minBound.z && pos.z <= maxBound.z) {
				result.push_back({pos, timestamp});
			}
		});
		return result;
	}

	size_t RocketVoxelTree::GetActiveCount() const {
		std::shared_lock lock(mutex_);
		return grid_.activeCellsCount();
	}

} // namespace Boidsish
