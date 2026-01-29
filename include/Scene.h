#ifndef SCENE_H
#define SCENE_H

#include "light.h"
#include "graphics.h"
#include <vector>
#include <string>
#include <optional>
#include <glm/glm.hpp>

namespace Boidsish {

	struct Scene {
		std::string           name;
		long long             timestamp;
		std::vector<Light>    lights;
		glm::vec3             ambient_light;
		std::optional<Camera> camera;
	};

} // namespace Boidsish

#endif // SCENE_H
