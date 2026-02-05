#pragma once

#include <bonxai/bonxai.hpp>
#include <glm/glm.hpp>
#include <shared_mutex>
#include <vector>

namespace Boidsish {

	/**
	 * @brief A single voxel data point for GPU transfer
	 */
	struct RocketVoxel {
		glm::vec3 position;
		float     timestamp;
	};

	/**
	 * @brief Thread-safe sparse voxel tree for rocket trails using Bonxai
	 *
	 * Each voxel stores the timestamp when it was last "activated" by a rocket.
	 * This allows the fragment shader to compute smoke density based on age.
	 */
	class RocketVoxelTree {
	public:
		/**
		 * @brief Construct a rocket voxel tree
		 * @param voxel_size Size of each voxel cell
		 */
		explicit RocketVoxelTree(double voxel_size = 1.0);

		~RocketVoxelTree() = default;

		// Non-copyable
		RocketVoxelTree(const RocketVoxelTree&) = delete;
		RocketVoxelTree& operator=(const RocketVoxelTree&) = delete;

		/**
		 * @brief Add a line segment of trail
		 * @param p1 Start position
		 * @param p2 End position
		 * @param time Current simulation time
		 */
		void AddSegment(const glm::vec3& p1, const glm::vec3& p2, float time);

		/**
		 * @brief Remove voxels that are older than maxAge
		 * @param currentTime Current simulation time
		 * @param maxAge Maximum age of a trail voxel
		 */
		void Prune(float currentTime, float maxAge);

		/**
		 * @brief Clear all voxels
		 */
		void Clear();

		/**
		 * @brief Get all active voxels for GPU upload
		 */
		std::vector<RocketVoxel> GetActiveVoxels() const;

		/**
		 * @brief Get active voxels within a specific bounding box
		 */
		std::vector<RocketVoxel> GetActiveVoxels(const glm::vec3& minBound, const glm::vec3& maxBound) const;

		/**
		 * @brief Tool to iterate over all active voxels
		 * @param func Function to call for each voxel: void(const glm::vec3& pos, float timestamp)
		 */
		template <typename Func>
		void ForEachActiveVoxel(Func&& func) const {
			std::shared_lock lock(mutex_);
			grid_.forEachCell([&](const float& timestamp, const Bonxai::CoordT& coord) {
				Bonxai::Point3D p = grid_.coordToPos(coord);
				func(glm::vec3(p.x, p.y, p.z), timestamp);
			});
		}

		/**
		 * @brief Get number of active voxels
		 */
		size_t GetActiveCount() const;

		/**
		 * @brief Get voxel size
		 */
		double GetVoxelSize() const { return voxel_size_; }

	private:
		double                   voxel_size_;
		Bonxai::VoxelGrid<float> grid_;
		mutable std::shared_mutex mutex_;
	};

} // namespace Boidsish
