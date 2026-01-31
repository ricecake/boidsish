#pragma once

#include <glm/glm.hpp>

namespace Boidsish {

	struct Plane {
		glm::vec3 normal;
		float     distance;
	};

	struct Frustum {
		Plane planes[6];

		bool IsBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const {
			for (int i = 0; i < 6; ++i) {
				const auto& plane = planes[i];
				// Find the corner most in the direction of the plane normal
				glm::vec3 positive_vertex(
					plane.normal.x >= 0 ? max.x : min.x,
					plane.normal.y >= 0 ? max.y : min.y,
					plane.normal.z >= 0 ? max.z : min.z
				);
				if (glm::dot(plane.normal, positive_vertex) + plane.distance < 0) {
					return false;
				}
			}
			return true;
		}
	};

} // namespace Boidsish
