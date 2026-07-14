#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include "dot.h"
#include "graphics.h"
#include "light.h"
#include "ConfigManager.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/VolumetricLightingEffect.h"
#include "weather_manager.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Clustered Forward Multi-Light Demo");

		// Enable Volumetric Lighting effect to emphasize the lights
		auto vol_effect = std::make_shared<Boidsish::PostProcessing::VolumetricLightingEffect>();
		vol_effect->SetIntensity(6.0f); // High intensity to emphasize volumetric light shafts
		vol_effect->SetScatteringAnisotropy(0.75f);
		vis.GetPostProcessingManager().AddEffect(vol_effect);

		// High haze and humidity to make light shafts fully visible
		auto weather = vis.GetWeatherManager();
		if (weather) {
			weather->SetTarget(Boidsish::WeatherAttribute::Humidity, 0.9f);
			weather->SetTarget(Boidsish::WeatherAttribute::HazeDensity, 3.5f);
		}

		// Configure time of day for night/twilight to make colored lights pop
		auto& light_mgr = vis.GetLightManager();
		auto& cycle = light_mgr.GetDayNightCycle();
		cycle.time = 22.0f; // 10 PM night
		cycle.speed = 0.0f; // Paused

		// Add 150 colored lights spread across the terrain and sky
		const std::vector<glm::vec3> colors = {
			glm::vec3(1.0f, 0.1f, 0.1f),   // Vibrant Red
			glm::vec3(0.1f, 1.0f, 0.1f),   // Vibrant Green
			glm::vec3(0.1f, 0.2f, 1.0f),   // Vibrant Blue
			glm::vec3(1.0f, 0.9f, 0.1f),   // Vibrant Yellow
			glm::vec3(0.9f, 0.1f, 1.0f),   // Vibrant Magenta
			glm::vec3(0.1f, 1.0f, 0.9f),   // Vibrant Cyan
			glm::vec3(1.0f, 0.5f, 0.0f),   // Vibrant Orange
			glm::vec3(0.5f, 0.0f, 1.0f)    // Vibrant Purple
		};

		// First, clear the default lights except the sun/moon if needed
		// Our custom scene will fully populate our own lights
		std::vector<int> light_ids;
		for (int i = 0; i < 150; ++i) {
			float angle = (float)i * 0.15f;
			float radius = 40.0f + (float)i * 1.5f;

			glm::vec3 pos;
			pos.x = std::cos(angle) * radius;
			pos.z = std::sin(angle) * radius;
			// Spread them vertically at different heights (terrain surface to high sky)
			pos.y = 10.0f + std::sin((float)i * 0.5f) * 25.0f;

			Boidsish::Light l = Boidsish::Light::Create(pos, 15.0f, colors[i % colors.size()], false);
			l.volumetric_shadow = true; // Enable volumetric rendering for these lights
			int id = vis.GetLightManager().AddLight(l);
			light_ids.push_back(id);
		}

		// Shape handler update callback to animate the lights dynamically
		float total_time = 0.0f;
		vis.AddShapeHandler([&, light_ids](float deltaTime) {
			total_time += deltaTime;
			std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

			// Retrieve and animate light positions dynamically
			auto& manager = vis.GetLightManager();
			for (size_t i = 0; i < light_ids.size(); ++i) {
				auto* light_ptr = manager.GetLight(light_ids[i]);
				if (light_ptr) {
					// Orbiting animation
					float base_angle = (float)i * 0.15f;
					float speed = 0.2f + (float)(i % 5) * 0.05f;
					float current_angle = base_angle + total_time * speed;
					float radius = 40.0f + (float)i * 1.5f;

					light_ptr->position.x = std::cos(current_angle) * radius;
					light_ptr->position.z = std::sin(current_angle) * radius;
					// Bobbing vertical animation
					light_ptr->position.y = 15.0f + std::sin(total_time * 1.5f + (float)i * 0.3f) * 20.0f;
				}
			}

			// Add a simple center monument
			auto center_sphere = std::make_shared<Boidsish::Dot>(0, 0, 10, 0, 20, 2.5f, 2.5f, 2.5f);
			center_sphere->SetUsePBR(true);
			center_sphere->SetRoughness(0.15f);
			center_sphere->SetMetallic(1.0f);
			center_sphere->SetColor(0.9f, 0.9f, 0.95f);
			shapes.push_back(center_sphere);

			return shapes;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
