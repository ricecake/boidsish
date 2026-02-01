#include "ui/PostProcessingWidget.h"

#include "imgui.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"

namespace Boidsish {
	namespace UI {

		PostProcessingWidget::PostProcessingWidget(PostProcessing::PostProcessingManager& manager): manager_(manager) {}

		void PostProcessingWidget::Draw() {
			ImGui::Begin("Post-Processing Effects");

			for (auto& effect : manager_.GetPreToneMappingEffects()) {
				bool is_enabled = effect->IsEnabled();
				if (ImGui::Checkbox(effect->GetName().c_str(), &is_enabled)) {
					effect->SetEnabled(is_enabled);
				}

				if (effect->GetName() == "Film Grain" && is_enabled) {
					auto film_grain_effect = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect);
					if (film_grain_effect) {
						float intensity = film_grain_effect->GetIntensity();
						if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f)) {
							film_grain_effect->SetIntensity(intensity);
						}
					}
				}

				if (effect->GetName() == "Atmosphere" && is_enabled) {
					auto atmosphere_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
					if (atmosphere_effect) {
						float haze_density = atmosphere_effect->GetHazeDensity();
						if (ImGui::SliderFloat("Haze Density", &haze_density, 0.0f, 0.05f, "%.4f")) {
							atmosphere_effect->SetHazeDensity(haze_density);
						}
						float haze_height = atmosphere_effect->GetHazeHeight();
						if (ImGui::SliderFloat("Haze Height", &haze_height, 0.0f, 100.0f)) {
							atmosphere_effect->SetHazeHeight(haze_height);
						}
						glm::vec3 haze_color = atmosphere_effect->GetHazeColor();
						if (ImGui::ColorEdit3("Haze Color", &haze_color[0])) {
							atmosphere_effect->SetHazeColor(haze_color);
						}
						float haze_g = atmosphere_effect->GetHazeG();
						if (ImGui::SliderFloat("Haze Anisotropy", &haze_g, 0.0f, 0.99f)) {
							atmosphere_effect->SetHazeG(haze_g);
						}
						float cloud_density = atmosphere_effect->GetCloudDensity();
						if (ImGui::SliderFloat("Cloud Density", &cloud_density, 0.0f, 1.0f)) {
							atmosphere_effect->SetCloudDensity(cloud_density);
						}
						float cloud_altitude = atmosphere_effect->GetCloudAltitude();
						if (ImGui::SliderFloat("Cloud Altitude", &cloud_altitude, 0.0f, 200.0f)) {
							atmosphere_effect->SetCloudAltitude(cloud_altitude);
						}
						float cloud_thickness = atmosphere_effect->GetCloudThickness();
						if (ImGui::SliderFloat("Cloud Thickness", &cloud_thickness, 0.0f, 50.0f)) {
							atmosphere_effect->SetCloudThickness(cloud_thickness);
						}
						glm::vec3 cloud_color = atmosphere_effect->GetCloudColor();
						if (ImGui::ColorEdit3("Cloud Color", &cloud_color[0])) {
							atmosphere_effect->SetCloudColor(cloud_color);
						}
						float cloud_g = atmosphere_effect->GetCloudG();
						if (ImGui::SliderFloat("Cloud Anisotropy", &cloud_g, 0.0f, 0.99f)) {
							atmosphere_effect->SetCloudG(cloud_g);
						}
					}
				}

				if (effect->GetName() == "Bloom" && is_enabled) {
					auto bloom_effect = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect);
					if (bloom_effect) {
						float intensity = bloom_effect->GetIntensity();
						if (ImGui::SliderFloat("Intensity##Bloom", &intensity, 0.0f, 2.0f)) {
							bloom_effect->SetIntensity(intensity);
						}
						float threshold = bloom_effect->GetThreshold();
						if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 3.0f)) {
							bloom_effect->SetThreshold(threshold);
						}
					}
				}
			}

			if (auto effect = manager_.GetToneMappingEffect()) {
				auto tone_mapping_effect = std::dynamic_pointer_cast<PostProcessing::ToneMappingEffect>(effect);
				ImGui::Separator();
				bool is_enabled = tone_mapping_effect->IsEnabled();
				if (is_enabled) {
					// Camera mode dropdown
					const char* modes[] =
						{"ACES", "Filmic", "Lottes", "Reinhard", "Reinhard II", "Uchimura", "Uncharted 2", "Unreal 3"};
					int current_mode = tone_mapping_effect->GetMode();
					if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
						tone_mapping_effect->SetMode(current_mode);
					}
				}
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
