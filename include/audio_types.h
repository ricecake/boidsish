#pragma once

#include <glm/glm.hpp>

namespace Boidsish {

	struct AudioState {
		glm::vec3 listener_pos{0.0f};
		glm::vec3 listener_front{0.0f, 0.0f, -1.0f};
		glm::vec3 listener_up{0.0f, 1.0f, 0.0f};
		float     listener_speed = 0.0f;
		float     listener_fov = 90.0f;
		float     global_pitch = 1.0f;
		float     master_volume = 1.0f;
		float     music_volume = 1.0f;
		float     sfx_volume = 1.0f;

		// Weather data for effects
		glm::vec3 wind_velocity{0.0f};
		float     wind_strength = 0.0f;
	};

} // namespace Boidsish
