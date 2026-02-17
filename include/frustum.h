#pragma once

#include <glm/glm.hpp>

namespace Boidsish {

	struct Plane {
		glm::vec3 normal;
		float     distance;
	};

	/**
	 * @brief GPU-compatible structure for frustum data.
	 * Matches std140 layout in frustum.glsl.
	 * Padded to 256 bytes for GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT compliance.
	 */
	struct FrustumDataGPU {
		glm::vec4 planes[6];       // 96 bytes: xyz = normal, w = distance
		glm::vec3 camera_pos;      // 12 bytes: for LOD/fading
		float     padding = 0;     // 4 bytes: std140 alignment
		float     _ubo_padding[36] = {}; // 144 bytes: pad to 256 total for UBO offset alignment
	};

	struct Frustum {
		Plane planes[6];

		static Frustum FromViewProjection(const glm::mat4& view, const glm::mat4& projection) {
			Frustum   frustum;
			glm::mat4 vp = projection * view;

			// Left plane
			frustum.planes[0].normal.x = vp[0][3] + vp[0][0];
			frustum.planes[0].normal.y = vp[1][3] + vp[1][0];
			frustum.planes[0].normal.z = vp[2][3] + vp[2][0];
			frustum.planes[0].distance = vp[3][3] + vp[3][0];

			// Right plane
			frustum.planes[1].normal.x = vp[0][3] - vp[0][0];
			frustum.planes[1].normal.y = vp[1][3] - vp[1][0];
			frustum.planes[1].normal.z = vp[2][3] - vp[2][0];
			frustum.planes[1].distance = vp[3][3] - vp[3][0];

			// Bottom plane
			frustum.planes[2].normal.x = vp[0][3] + vp[0][1];
			frustum.planes[2].normal.y = vp[1][3] + vp[1][1];
			frustum.planes[2].normal.z = vp[2][3] + vp[2][1];
			frustum.planes[2].distance = vp[3][3] + vp[3][1];

			// Top plane
			frustum.planes[3].normal.x = vp[0][3] - vp[0][1];
			frustum.planes[3].normal.y = vp[1][3] - vp[1][1];
			frustum.planes[3].normal.z = vp[2][3] - vp[2][1];
			frustum.planes[3].distance = vp[3][3] - vp[3][1];

			// Near plane
			frustum.planes[4].normal.x = vp[0][3] + vp[0][2];
			frustum.planes[4].normal.y = vp[1][3] + vp[1][2];
			frustum.planes[4].normal.z = vp[2][3] + vp[2][2];
			frustum.planes[4].distance = vp[3][3] + vp[3][2];

			// Far plane
			frustum.planes[5].normal.x = vp[0][3] - vp[0][2];
			frustum.planes[5].normal.y = vp[1][3] - vp[1][2];
			frustum.planes[5].normal.z = vp[2][3] - vp[2][2];
			frustum.planes[5].distance = vp[3][3] - vp[3][2];

			// Normalize the planes
			for (int i = 0; i < 6; ++i) {
				float length = glm::length(frustum.planes[i].normal);
				frustum.planes[i].normal /= length;
				frustum.planes[i].distance /= length;
			}

			return frustum;
		}

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
