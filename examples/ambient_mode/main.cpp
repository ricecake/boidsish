#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <tuple>
#include <vector>

#include "ConfigManager.h"
#include "ambient_camera.h"
#include "decor_manager.h"
#include "dot.h"
#include "fire_effect.h"
#include "graphics.h"
#include "light.h"
#include "light_manager.h"
#include "logger.h"
#include "model.h"
#include "polyhedron.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AutoExposureEffect.h"
#include "procedural_generator.h"
#include "rigid_body.h"
#include "spline.h"
#include "terrain_generator.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

int main() {
	try {
		// Disable distracted elements
		ConfigManager::GetInstance().SetBool("enable_floor", false);
		ConfigManager::GetInstance().SetBool("enable_sky", false);

		Visualizer visualizer(1280, 720, "Ambient Mode: Alaska Sunset");
		std::srand(static_cast<unsigned int>(std::time(nullptr)));

		// --- State variables ---
		bool sun_on = true;

		// Prepare will be called by Run, but we can add callbacks
		visualizer.AddPrepareCallback([](Visualizer& viz) {
			logger::LOG("Ambient Mode preparing...");

			// Initial world scale
			auto terrain = viz.GetTerrain();
			if (terrain) {
				terrain->SetWorldScale(5.0f);
			}

			viz.SetCameraMode(CameraMode::FREE);

			// Alaska-style sunset setup
			auto& light_manager = viz.GetLightManager();
			auto& cycle = light_manager.GetDayNightCycle();
			cycle.enabled = false; // We will manually drive the sun

			// Initial sun position: at the horizon
			auto& lights = light_manager.GetLights();
			if (!lights.empty() && lights[0].type == DIRECTIONAL_LIGHT) {
				lights[0].azimuth = 180.0f;  // South/West
				lights[0].elevation = -1.0f; // Just below the horizon for sunset colors
				lights[0].UpdateDirectionFromAngles();
				lights[0].casts_shadow = false; // Night is dominant, disable shadows
			}

			// Decor: Small buildings in the trees
			auto decor_manager = viz.GetDecorManager();
			if (decor_manager) {
				DecorProperties props;
				props.SetDensity(0.05f); // Sparse
				props.base_scale = 5.0f;
				props.scale_variance = 0.5f;
				props.align_to_terrain = true;
				props.random_yaw = true;

				// Restrict to Grassland/Forest biomes (first 8 biomes are Grassland sub-biomes)
				for (int i = 0; i < 8; ++i) {
					props.biomes.set(static_cast<Biome>(i));
				}

				decor_manager->AddProceduralDecor(ProceduralType::Structure, props, 5);
			}
		});

		// --- Probe & Path State ---
		auto probe = std::make_shared<Boidsish::Polyhedron>(PolyhedronType::SmallStellatedDodecahedron, 0, 0, 0, 0);
		probe->SetColor(0.2f, 0.8f, 1.0f, 0.5f);
		probe->SetScale(glm::vec3(5.0f));
		probe->SetRefractive(true, 1.5f);
		probe->SetTrailLength(0);

		AmbientCameraSystem ambient_system;

		int       probe_light_id = -1;
		float     last_bubble_time = 0.0f;
		glm::vec3 probe_pos(0.0f, 50.0f, 0.0f);

		// Use custom destination callback to maintain "firefly biome" preference
		ambient_system.SetNextDestinationCallback([](ITerrainGenerator* terrain) {
			if (!terrain)
				return glm::vec3(0, 0, 0);

			glm::vec3 bestDest(0, 0, 0);
			float     bestScore = -1.0f;

			for (int i = 0; i < 30; ++i) {
				float rx = (std::rand() % 10000 - 5000);
				float rz = (std::rand() % 10000 - 5000);

				float control = terrain->GetBiomeControlValue(rx, rz);
				// Lush biomes preference (control ~ 0.25)
				float score = 1.0f - std::abs(control - 0.25f);

				if (score > bestScore) {
					bestScore = score;
					bestDest = glm::vec3(rx, 0, rz);
				}
			}

			logger::LOG(
				"Firefly biome destination selected at ",
				bestDest.x,
				", ",
				bestDest.z,
				" (score: ",
				bestScore,
				")"
			);
			return bestDest;
		});

		// Driving sun orbit and night factor override, and Probe logic
		visualizer.AddShapeHandler([&](float time) {
			auto& light_manager = visualizer.GetLightManager();
			auto& cycle = light_manager.GetDayNightCycle();
			cycle.night_factor = 1.0f; // Maximum fireflies

			auto& lights = light_manager.GetLights();
			if (!lights.empty() && lights[0].type == DIRECTIONAL_LIGHT) {
				// Sun slowly orbits the horizon
				float sun_orbit_speed = 0.05f;
				lights[0].azimuth = 180.0f + 90.0f * std::sin(time * sun_orbit_speed);
				lights[0].elevation = -0.5f + 1.5f * std::cos(time * sun_orbit_speed * 0.7f);
				lights[0].UpdateDirectionFromAngles();

				lights[0].base_intensity = sun_on ? 1.0f : 0.0f;
			}

			// --- Path & Probe Movement ---
			float dt = visualizer.GetLastFrameTime();
			auto  terrain = visualizer.GetTerrain();
			auto  decor = visualizer.GetDecorManager();
			auto& cam = visualizer.GetCamera();

			ambient_system.Update(dt, terrain.get(), decor, cam, probe_pos);

			probe->SetPosition(probe_pos.x, probe_pos.y, probe_pos.z);
			probe->SetRotation(glm::angleAxis(time, glm::vec3(0, 1, 0.5f)));

			// --- Probe Light ---
			if (probe_light_id == -1) {
				probe_light_id = light_manager.AddLight(
					Light::CreateEmissive(probe_pos, 5.0f, {0.2f, 0.8f, 1.0f}, 1.0f)
				);
			} else {
				auto light = light_manager.GetLight(probe_light_id);
				if (light) {
					light->position = probe_pos;
					light->intensity = 5.0f + 2.0f * std::sin(time * 5.0f); // Pulsing glow
				}
			}

			// --- Bubble Effect ---
			if (time - last_bubble_time > 5.0f) {
				visualizer.AddFireEffect(probe_pos, FireEffectStyle::Bubbles, {0, 1, 0}, {0, 2, 0}, 20, 3.0f);
				last_bubble_time = time;
			}

			return std::vector<std::shared_ptr<Boidsish::Shape>>{probe};
		});

		// --- Input Controls ---
		visualizer.AddInputCallback([&](const Boidsish::InputState& state) {
			auto&                                               post_manager = visualizer.GetPostProcessingManager();
			std::shared_ptr<PostProcessing::AutoExposureEffect> exposure;

			for (auto& effect : post_manager.GetPreToneMappingEffects()) {
				if (auto ae = std::dynamic_pointer_cast<PostProcessing::AutoExposureEffect>(effect)) {
					exposure = ae;
					break;
				}
			}

			if (exposure) {
				float lum = exposure->GetTargetLuminance();
				if (state.keys[GLFW_KEY_UP])
					lum += 0.1f * state.delta_time;
				if (state.keys[GLFW_KEY_DOWN])
					lum -= 0.1f * state.delta_time;
				exposure->SetTargetLuminance(std::clamp(lum, 0.05f, 2.0f));
			}

			if (state.key_down[GLFW_KEY_SPACE]) {
				sun_on = !sun_on;
			}
		});

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
