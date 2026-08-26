#include "ui/AmbientSettingsComponent.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "light_manager.h"

namespace Boidsish {
	namespace UI {

		void AmbientSettingsComponent::Draw(Visualizer& visualizer) {
			auto& light_manager = visualizer.GetLightManager();
			auto& config_manager = ConfigManager::GetInstance();

			// Source Radiance & SH Probe Exposure Scaling
			float sky_exposure = light_manager.GetSkyExposure();
			if (ImGui::SliderFloat("Sky Exposure Multiplier", &sky_exposure, 0.0f, 10.0f, "%.2f")) {
				light_manager.SetSkyExposure(sky_exposure);
				config_manager.SetFloat("sky_exposure", sky_exposure);
			}

			float star_exposure = light_manager.GetStarExposure();
			if (ImGui::SliderFloat("Star & Nebula Exposure Multiplier", &star_exposure, 0.0f, 10.0f, "%.2f")) {
				light_manager.SetStarExposure(star_exposure);
				config_manager.SetFloat("star_exposure", star_exposure);
			}

			float terrain_exposure = light_manager.GetTerrainExposure();
			if (ImGui::SliderFloat("Terrain Bounce Exposure Multiplier", &terrain_exposure, 0.0f, 10.0f, "%.2f")) {
				light_manager.SetTerrainExposure(terrain_exposure);
				config_manager.SetFloat("terrain_exposure", terrain_exposure);
			}

			float probe_scaling = light_manager.GetProbeScaling();
			if (ImGui::SliderFloat("SH Probe Multiplier", &probe_scaling, 0.0f, 5.0f, "%.2f")) {
				light_manager.SetProbeScaling(probe_scaling);
				config_manager.SetFloat("sh_probe_scaling", probe_scaling);
			}

			float probe_convergence = light_manager.GetProbeConvergenceSpeed();
			if (ImGui::SliderFloat("SH Convergence Speed", &probe_convergence, 0.1f, 10.0f)) {
				light_manager.SetProbeConvergenceSpeed(probe_convergence);
				config_manager.SetFloat("sh_probe_convergence_speed", probe_convergence);
			}

			int probe_ray_multiplier = light_manager.GetProbeRayCountMultiplier();
			if (ImGui::SliderInt("SH Quality (Ray Multiplier)", &probe_ray_multiplier, 1, 8)) {
				light_manager.SetProbeRayCountMultiplier(probe_ray_multiplier);
				config_manager.SetInt("sh_probe_ray_count_multiplier", probe_ray_multiplier);
			}
		}

	} // namespace UI
} // namespace Boidsish
