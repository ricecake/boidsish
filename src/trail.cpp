#include "trail.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "shape.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	Trail::Trail(int max_length, float thickness): max_length(max_length), thickness(thickness), mesh_dirty(false) {}

	Trail::~Trail() {}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		if (full) {
			points[tail] = {position, color};
			head = (head + 1) % max_length;
			tail = (tail + 1) % max_length;
		} else {
			points.push_back({position, color});
			tail = points.size() % max_length;
			if (points.size() == static_cast<size_t>(max_length)) {
				full = true;
				tail = 0;
			}
		}

		mesh_dirty = true;
		bounds_dirty_ = true;
	}

	void Trail::SetIridescence(bool enabled) {
		iridescent_ = enabled;
	}

	void Trail::SetUseRocketTrail(bool enabled) {
		useRocketTrail_ = enabled;
	}

	glm::vec3 Trail::GetMinBound() const {
		if (bounds_dirty_) {
			UpdateBounds();
		}
		return min_bound_;
	}

	glm::vec3 Trail::GetMaxBound() const {
		if (bounds_dirty_) {
			UpdateBounds();
		}
		return max_bound_;
	}

	void Trail::UpdateBounds() const {
		if (points.empty()) {
			min_bound_ = max_bound_ = glm::vec3(0.0f);
			bounds_dirty_ = false;
			return;
		}

		min_bound_ = points[0].first;
		max_bound_ = points[0].first;

		for (const auto& p : points) {
			min_bound_ = glm::min(min_bound_, p.first);
			max_bound_ = glm::max(max_bound_, p.first);
		}

		// Expand by thickness to account for the trail's volume
		min_bound_ -= glm::vec3(thickness);
		max_bound_ += glm::vec3(thickness);

		bounds_dirty_ = false;
	}

} // namespace Boidsish
