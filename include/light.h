#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

namespace Boidsish {

	struct Light {
		glm::vec3 position;
		float     intensity;
		glm::vec3 color;
		float     padding; // for std140 alignment
	};

} // namespace Boidsish

#endif // LIGHT_H
