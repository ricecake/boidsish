#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "graphics.h"
#include "light.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "terrain_generator_interface.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Cloud Lighting Demo");

		// Get Atmosphere effect to access weather map
		auto& pp_manager = vis.GetPostProcessingManager();
		std::shared_ptr<Boidsish::PostProcessing::AtmosphereEffect> atmosphere;
		for (auto& effect : pp_manager.GetPreToneMappingEffects()) {
			if (effect->GetName() == "Atmosphere") {
				atmosphere = std::dynamic_pointer_cast<Boidsish::PostProcessing::AtmosphereEffect>(effect);
				break;
			}
		}

		if (!atmosphere) {
			std::cerr << "Atmosphere effect not found!" << std::endl;
			return 1;
		}

		// Configure clouds for the demo
		atmosphere->SetCloudCoverage(0.6f);
		atmosphere->SetCloudDensity(0.5f);

		// Add a central light that we will position inside a cloud
		Boidsish::Light cloudLight = Boidsish::Light::Create(glm::vec3(0, 500, 0), 50.0f, glm::vec3(1.0f, 0.8f, 0.4f), true);
		cloudLight.SetFlicker(2.0f);
		cloudLight.volumetric_shadow = true;
		cloudLight.camera_relative = true; // Make it relative so we can keep it "near" the camera's world region

		int lightId = vis.GetLightManager().AddLight(cloudLight);

		bool positionFound = false;

		vis.AddUpdateHandler([&](float dt, float totalTime) {
			if (positionFound) return;

			GLuint tex = 0.0;//atmosphere->GetCloudWeatherTexture();
			if (tex == 0) return;

			// Read back a small portion of the 2048x2048 texture to find a cloud
			// For simplicity in a demo, we'll just read the whole thing once
			static std::vector<float> data(2048 * 2048 * 4);
			glBindTexture(GL_TEXTURE_2D, tex);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data.data());

			// Search for a cloud (negative SDF) near the "center" of the map
			// Map UV (0.5, 0.5) corresponds to world (0, 0) if we assume the map is centered
			// The bake shader uses: vec2 uv = p_advected.xz / (100000.0 * props.worldScale);
			// So world (0,0) is UV (0,0).

			for (int y = 0; y < 100; ++y) {
				for (int x = 0; x < 100; ++x) {
					int idx = (y * 2048 + x) * 4;
					float sdf = data[idx];
					if (sdf < -500.0f) { // Significant cloud density
						float worldScale = vis.GetTerrain() ? vis.GetTerrain()->GetWorldScale() : 1.0f;
						float u = (float)x / 2048.0f;
						float v = (float)y / 2048.0f;

						glm::vec3 offset;
						offset.x = u * 100000.0f * worldScale;
						offset.z = v * 100000.0f * worldScale;
						offset.y = atmosphere->GetCloudAltitude() * worldScale + 100.0f * worldScale;

						auto* light = vis.GetLightManager().GetLight(lightId);
						if (light) {
							light->position = offset;
							positionFound = true;
							std::cout << "Placed cloud light at relative offset: " << offset.x << ", " << offset.y << ", " << offset.z << std::endl;
						}
						return;
					}
				}
			}
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
