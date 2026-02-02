#include <iostream>
#include <memory>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "graphics.h"
#include "light.h"

// Helper functions for demo
float clamp_val(float x, float minVal, float maxVal) {
	return std::min(std::max(x, minVal), maxVal);
}

float smoothstep_val(float edge0, float edge1, float x) {
	float t = clamp_val((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Sky Scattering Demo");

		// Remove default lights and add a controllable sun
		vis.GetLightManager().GetLights().clear();

		Boidsish::Light sun = Boidsish::Light::CreateDirectional(
			glm::vec3(0, 500, 0),
			glm::vec3(0, -1, 0),
			5.0f,
			glm::vec3(1.0f, 0.95f, 0.8f),
			true
		);
		vis.GetLightManager().AddLight(sun);

		// Initial camera setup
		vis.AddPrepareCallback([](Boidsish::Visualizer& v) {
			v.GetCamera().y = 5.0f;
			v.GetCamera().z = 20.0f;
			v.GetCamera().pitch = 15.0f;
			v.GetCamera().yaw = 0.0f;
		});

		// Interaction to control time of day
		vis.AddInputCallback([&](const Boidsish::InputState& state) {
			static float time_of_day = 0.15f; // Start early morning
			static bool  auto_advance = true;

			if (state.key_down[GLFW_KEY_T])
				auto_advance = !auto_advance;

			if (auto_advance) {
				time_of_day += state.delta_time * 0.01f;
			}

			if (state.keys[GLFW_KEY_UP])
				time_of_day += state.delta_time * 0.1f;
			if (state.keys[GLFW_KEY_DOWN])
				time_of_day -= state.delta_time * 0.1f;

			if (time_of_day > 1.0f)
				time_of_day -= 1.0f;
			if (time_of_day < 0.0f)
				time_of_day += 1.0f;

			// Convert time of day [0,1] to sun direction
			// 0.0 = Sunrise (East), 0.25 = Noon (Zenith), 0.5 = Sunset (West), 0.75 = Midnight (Nadir)
			float     angle = time_of_day * 2.0f * 3.14159f;
			glm::vec3 dir(cos(angle), -sin(angle), 0.0f);

			auto& lights = vis.GetLightManager().GetLights();
			if (!lights.empty()) {
				lights[0].direction = dir;

				float elevation = -dir.y;
				// Adjust sun color and intensity based on elevation
				if (elevation > -0.1f) {
					float t = smoothstep_val(-0.1f, 0.2f, elevation);
					glm::vec3 sunset_color(1.0f, 0.4f, 0.1f);
					glm::vec3 day_color(1.0f, 0.95f, 0.85f);
					lights[0].color = sunset_color + (day_color - sunset_color) * t;
					lights[0].intensity = 5.0f * smoothstep_val(-0.1f, 0.1f, elevation);
				} else {
					lights[0].intensity = 0.0f;
				}

				// Adjust ambient light
				glm::vec3 ambient_day(0.2f, 0.25f, 0.35f);
				glm::vec3 ambient_night(0.02f, 0.02f, 0.05f);
				float     amb_t = smoothstep_val(-0.2f, 0.2f, elevation);
				vis.GetLightManager().SetAmbientLight(ambient_night + (ambient_day - ambient_night) * amb_t);
			}

			if (state.key_down[GLFW_KEY_L]) {
				std::cout << "Time of Day: " << time_of_day << " Sun Dir: (" << dir.x << ", " << dir.y << ", " << dir.z
						  << ")" << std::endl;
			}
		});

		std::cout << "Sky Scattering Demo Controls:" << std::endl;
		std::cout << "  UP/DOWN: Manually change time of day" << std::endl;
		std::cout << "  T: Toggle auto-advance time" << std::endl;
		std::cout << "  L: Log current sun state" << std::endl;
		std::cout << "  WASD: Move camera" << std::endl;
		std::cout << "  Mouse: Look around" << std::endl;

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
