#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

namespace Boidsish {

	struct Light {
		glm::vec3 position;
		float     intensity;
		glm::vec3 color;
		int       casts_shadow; // Using int for bool
		glm::mat4 lightSpaceMatrix;
	};

} // namespace Boidsish

#endif // LIGHT_H
