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

			// SH Probe Ambient Scaling
			float probe_scaling = light_manager.GetProbeScaling();
			if (ImGui::SliderFloat("Ambient Intensity (SH)", &probe_scaling, 0.0f, 5.0f)) {
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
