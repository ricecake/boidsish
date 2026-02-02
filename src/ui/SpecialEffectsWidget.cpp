#include "ui/SpecialEffectsWidget.h"
#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"

namespace Boidsish {
	namespace UI {
		SpecialEffectsWidget::SpecialEffectsWidget(Visualizer* visualizer): m_visualizer(visualizer) {}
		void SpecialEffectsWidget::Draw() {
			if (!m_show) return;
			ImGui::Begin("Special Effects", &m_show);
			auto& config = ConfigManager::GetInstance();

			if (ImGui::CollapsingHeader("Object Level", ImGuiTreeNodeFlags_DefaultOpen)) {
				// 2. Checkboxes
				auto decor_manager = m_visualizer->GetDecorManager();
				if (decor_manager) {
					bool foliage = decor_manager->IsEnabled();
					if (ImGui::Checkbox("Foliage", &foliage)) decor_manager->SetEnabled(foliage);
				}
				bool ripple = config.GetAppSettingBool("artistic_effect_ripple", false);
				if (ImGui::Checkbox("Ripple", &ripple)) config.SetBool("artistic_effect_ripple", ripple);
				bool color_shift = config.GetAppSettingBool("artistic_effect_color_shift", false);
				if (ImGui::Checkbox("Color Shift", &color_shift)) config.SetBool("artistic_effect_color_shift", color_shift);
				bool bnw = config.GetAppSettingBool("artistic_effect_black_and_white", false);
				if (ImGui::Checkbox("Black and White", &bnw)) config.SetBool("artistic_effect_black_and_white", bnw);
				bool negative = config.GetAppSettingBool("artistic_effect_negative", false);
				if (ImGui::Checkbox("Negative (Artistic)", &negative)) config.SetBool("artistic_effect_negative", negative);
				bool shimmery = config.GetAppSettingBool("artistic_effect_shimmery", false);
				if (ImGui::Checkbox("Shimmery", &shimmery)) config.SetBool("artistic_effect_shimmery", shimmery);
				bool glitched = config.GetAppSettingBool("artistic_effect_glitched", false);
				if (ImGui::Checkbox("Glitched", &glitched)) config.SetBool("artistic_effect_glitched", glitched);
				bool wireframe = config.GetAppSettingBool("artistic_effect_wireframe", false);
				if (ImGui::Checkbox("Wireframe", &wireframe)) config.SetBool("artistic_effect_wireframe", wireframe);
			}

			if (ImGui::CollapsingHeader("Full Screen", ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& pp_manager = m_visualizer->GetPostProcessingManager();
				auto& effects = pp_manager.GetPreToneMappingEffects();

				// 1. Dropdowns (Multi-property effects)
				for (auto& effect : effects) {
					std::string name = effect->GetName();
					if (name == "Bloom" || name == "Atmosphere" || name == "Film Grain") {
						if (ImGui::TreeNode(name.c_str())) {
							bool enabled = effect->IsEnabled();
							if (ImGui::Checkbox("Enabled", &enabled)) effect->SetEnabled(enabled);
							if (name == "Film Grain") {
								auto fg = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect);
								float intensity = fg->GetIntensity();
								if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f)) fg->SetIntensity(intensity);
							} else if (name == "Atmosphere") {
								auto atmos = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
								float haze_d = atmos->GetHazeDensity();
								if (ImGui::SliderFloat("Haze Density", &haze_d, 0.0f, 0.05f)) atmos->SetHazeDensity(haze_d);
								float haze_h = atmos->GetHazeHeight();
								if (ImGui::SliderFloat("Haze Height", &haze_h, 0.0f, 100.0f)) atmos->SetHazeHeight(haze_h);
								glm::vec3 haze_c = atmos->GetHazeColor();
								if (ImGui::ColorEdit3("Haze Color", &haze_c[0])) atmos->SetHazeColor(haze_c);
								float cloud_d = atmos->GetCloudDensity();
								if (ImGui::SliderFloat("Cloud Density", &cloud_d, 0.0f, 1.0f)) atmos->SetCloudDensity(cloud_d);
							} else if (name == "Bloom") {
								auto bloom = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect);
								float intensity = bloom->GetIntensity();
								if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 2.0f)) bloom->SetIntensity(intensity);
								float threshold = bloom->GetThreshold();
								if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 3.0f)) bloom->SetThreshold(threshold);
							}
							ImGui::TreePop();
						}
					}
				}
				if (auto tm = pp_manager.GetToneMappingEffect()) {
					if (ImGui::TreeNode("Tone Mapping")) {
						bool enabled = tm->IsEnabled();
						if (ImGui::Checkbox("Enabled", &enabled)) tm->SetEnabled(enabled);
						const char* modes[] = {"ACES", "Filmic", "Lottes", "Reinhard", "Reinhard II", "Uchimura", "Uncharted 2", "Unreal 3"};
						auto tm_effect = std::dynamic_pointer_cast<PostProcessing::ToneMappingEffect>(tm);
						int current_mode = tm_effect->GetMode();
						if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) tm_effect->SetMode(current_mode);
						ImGui::TreePop();
					}
				}

				// 2. Checkboxes (Single-property effects)
				for (auto& effect : effects) {
					std::string name = effect->GetName();
					if (!(name == "Bloom" || name == "Atmosphere" || name == "Film Grain")) {
						bool enabled = effect->IsEnabled();
						if (ImGui::Checkbox(name.c_str(), &enabled)) effect->SetEnabled(enabled);
					}
				}
			}
			ImGui::End();
		}
	}
}
