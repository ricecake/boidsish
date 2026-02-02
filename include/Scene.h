#ifndef SCENE_H
#define SCENE_H

#include <optional>
#include <string>
#include <vector>

#include "graphics.h"
#include "light.h"
#include <glm/glm.hpp>

namespace Boidsish {

	struct Scene {
		std::string           name;
		long long             timestamp;
		std::vector<Light>    lights;
		glm::vec3             ambient_light;
		std::optional<Camera> camera;

		// Object-level artistic effects
		struct ObjectEffects {
			bool ripple = false;
			bool color_shift = false;
			bool black_and_white = false;
			bool negative = false;
			bool shimmery = false;
			bool glitched = false;
			bool wireframe = false;
		} object_effects;

		// Post-processing effects settings
		struct PostProcessingSettings {
			bool  bloom_enabled = false;
			float bloom_intensity = 0.1f;
			float bloom_threshold = 1.0f;

			bool      atmosphere_enabled = true;
			float     haze_density = 0.005f;
			float     haze_height = 20.0f;
			glm::vec3 haze_color = glm::vec3(0.6f, 0.7f, 0.8f);
			float     cloud_density = 0.5f;
			float     cloud_altitude = 95.0f;
			float     cloud_thickness = 10.0f;
			glm::vec3 cloud_color = glm::vec3(0.95f, 0.95f, 1.0f);
			float     scattering_strength = 0.2f;
			float     atmosphere_exposure = 0.3f;

			bool tone_mapping_enabled = false;

			bool  film_grain_enabled = false;
			float film_grain_intensity = 0.02f;

			bool  ssao_enabled = false;
			float ssao_radius = 0.5f;
			float ssao_bias = 0.1f;
			float ssao_intensity = 1.0f;
			float ssao_power = 1.0f;

			bool negative_enabled = false;
			bool glitch_enabled = false;
			bool optical_flow_enabled = false;
			bool strobe_enabled = false;
			bool whisp_trail_enabled = false;
			bool time_stutter_enabled = false;
		} post_processing;
	};

} // namespace Boidsish

#endif // SCENE_H
