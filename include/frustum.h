#pragma once

#include <glm/glm.hpp>

namespace Boidsish {

	struct Plane {
		glm::vec3 normal;
		float     distance;
	};

	struct Frustum {
		Plane planes[6];
	};

} // namespace Boidsish
