#pragma once

#include <vector>

#include "vector.h"
#include <glm/glm.hpp>

namespace Boidsish {
	namespace Spline {

		Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3);
		Vector3
		CatmullRomDerivative(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3);

		struct VertexData {
			glm::vec3 pos, normal, color;
		};

		std::vector<VertexData> GenerateTube(
			const std::vector<Vector3>&   points,
			const std::vector<Vector3>&   ups,
			const std::vector<float>&     sizes,
			const std::vector<glm::vec3>& colors,
			bool                          is_looping,
			int                           curve_segments = 10,
			int                           cylinder_segments = 12
		);

	} // namespace Spline
} // namespace Boidsish
