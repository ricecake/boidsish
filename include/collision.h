#pragma once
#include <algorithm>
#include <limits>

#include <glm/common.hpp>
#include <glm/glm.hpp>

namespace Boidsish {

	struct Ray {
		glm::vec3 origin;
		glm::vec3 direction;

		Ray(const glm::vec3& o, const glm::vec3& d): origin(o), direction(glm::normalize(d)) {}
	};

	struct AABB {
		glm::vec3 min;
		glm::vec3 max;

		AABB(): min(0.0f), max(0.0f) {}

		AABB(const glm::vec3& min, const glm::vec3& max): min(min), max(max) {}

		bool Intersects(const Ray& ray, float& t) const {
			glm::vec3 invDir = 1.0f / ray.direction;
			glm::vec3 t0 = (min - ray.origin) * invDir;
			glm::vec3 t1 = (max - ray.origin) * invDir;

			glm::vec3 tmin = glm::min(t0, t1);
			glm::vec3 tmax = glm::max(t0, t1);

			float fmin = std::max(std::max(tmin.x, tmin.y), tmin.z);
			float fmax = std::min(std::min(tmax.x, tmax.y), tmax.z);

			if (fmax < 0 || fmin > fmax)
				return false;
			t = fmin;
			return true;
		}

		AABB Transform(const glm::mat4& matrix) const {
			glm::vec3 corners[8] = {
				{min.x, min.y, min.z},
				{min.x, min.y, max.z},
				{min.x, max.y, min.z},
				{min.x, max.y, max.z},
				{max.x, min.y, min.z},
				{max.x, min.y, max.z},
				{max.x, max.y, min.z},
				{max.x, max.y, max.z}
			};
			glm::vec3 newMin(std::numeric_limits<float>::max());
			glm::vec3 newMax(-std::numeric_limits<float>::max());
			for (int i = 0; i < 8; ++i) {
				glm::vec3 transformed = glm::vec3(matrix * glm::vec4(corners[i], 1.0f));
				newMin = glm::min(newMin, transformed);
				newMax = glm::max(newMax, transformed);
			}
			return AABB(newMin, newMax);
		}

		bool IsEmpty() const { return min.x >= max.x || min.y >= max.y || min.z >= max.z; }

		glm::vec3 GetCorner(int i) const {
			return glm::vec3(
				(i & 1) ? max.x : min.x,
				(i & 2) ? max.y : min.y,
				(i & 4) ? max.z : min.z
			);
		}
	};

} // namespace Boidsish
