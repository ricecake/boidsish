#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

namespace Boidsish {

	/**
	 * @brief Represents the horizon from a specific point, storing maximum elevation slopes
	 * in multiple directions (sectors). Used for viewshed approximation and approach pathing.
	 */
	struct Horizon {
		static constexpr int kNumSectors = 64;
		float                max_slopes[kNumSectors];
		glm::vec3            origin;

		Horizon() {
			for (int i = 0; i < kNumSectors; ++i) {
				max_slopes[i] = -1.0f; // Minimal possible slope
			}
			origin = glm::vec3(0.0f);
		}

		/**
		 * @brief Get the maximum slope to the horizon in a specific world-space direction.
		 * @param direction Direction vector (need not be normalized)
		 * @return The maximum slope (tan of elevation angle) in that direction.
		 */
		float GetMaxSlope(const glm::vec3& direction) const {
			float angle = atan2(direction.z, direction.x);
			// Map angle [-pi, pi] to [0, kNumSectors)
			float sector_f = (angle + glm::pi<float>()) / (2.0f * glm::pi<float>()) * (float)kNumSectors;
			int   sector = (int)floor(sector_f) % kNumSectors;
			if (sector < 0)
				sector += kNumSectors;
			return max_slopes[sector];
		}

		/**
		 * @brief Check if a point in space is likely visible from the horizon's origin.
		 * @param point The world-space point to check.
		 * @return true if the point is above the horizon.
		 */
		bool IsVisible(const glm::vec3& point) const {
			glm::vec3 diff = point - origin;
			float     dist_sq = diff.x * diff.x + diff.z * diff.z;
			if (dist_sq < 1e-4f)
				return true; // Very close points are considered visible

			float dist = sqrt(dist_sq);
			float slope = diff.y / dist;

			return slope > GetMaxSlope(diff);
		}
	};

} // namespace Boidsish
