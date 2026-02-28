#include "ui/EffectWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AutoExposureEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/GtaoEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"

namespace Boidsish {
	namespace UI {

		EffectWidget::EffectWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void EffectWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(540, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Effects", &m_show)) {
				// 1. Artistic Effects (from EffectsWidget)
				if (ImGui::CollapsingHeader("Artistic Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();

					bool ripple_enabled = config.GetAppSettingBool("artistic_effect_ripple", false);
					if (ImGui::Checkbox("Ripple", &ripple_enabled)) {
						config.SetBool("artistic_effect_ripple", ripple_enabled);
					}

					bool color_shift_enabled = config.GetAppSettingBool("artistic_effect_color_shift", false);
					if (ImGui::Checkbox("Color Shift", &color_shift_enabled)) {
						config.SetBool("artistic_effect_color_shift", color_shift_enabled);
					}

					bool bnw_enabled = config.GetAppSettingBool("artistic_effect_black_and_white", false);
					if (ImGui::Checkbox("Black and White", &bnw_enabled)) {
						config.SetBool("artistic_effect_black_and_white", bnw_enabled);
					}

					bool negative_enabled = config.GetAppSettingBool("artistic_effect_negative", false);
					if (ImGui::Checkbox("Negative (Artistic)", &negative_enabled)) {
						config.SetBool("artistic_effect_negative", negative_enabled);
					}

					bool shimmery_enabled = config.GetAppSettingBool("artistic_effect_shimmery", false);
					if (ImGui::Checkbox("Shimmery", &shimmery_enabled)) {
						config.SetBool("artistic_effect_shimmery", shimmery_enabled);
					}

					bool glitched_enabled = config.GetAppSettingBool("artistic_effect_glitched", false);
					if (ImGui::Checkbox("Glitched", &glitched_enabled)) {
						config.SetBool("artistic_effect_glitched", glitched_enabled);
					}

					bool wireframe_enabled = config.GetAppSettingBool("artistic_effect_wireframe", false);
					if (ImGui::Checkbox("Wireframe", &wireframe_enabled)) {
						config.SetBool("artistic_effect_wireframe", wireframe_enabled);
					}

					bool cloak_enabled = config.GetAppSettingBool("artistic_effect_cloak", false);
					if (ImGui::Checkbox("Cloak", &cloak_enabled)) {
						config.SetBool("artistic_effect_cloak", cloak_enabled);
					}
				}

				// 2. Post-Processing Effects (from PostProcessingWidget)
				if (ImGui::CollapsingHeader("Post-Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& manager = m_visualizer.GetPostProcessingManager();

					for (auto& effect : manager.GetPreToneMappingEffects()) {
						// Atmosphere is handled in EnvironmentWidget
						if (effect->GetName() == "Atmosphere")
							continue;

						bool is_enabled = effect->IsEnabled();
						if (ImGui::Checkbox(effect->GetName().c_str(), &is_enabled)) {
							effect->SetEnabled(is_enabled);
						}

						if (effect->GetName() == "AutoExposure" && is_enabled) {
							auto auto_exposure_effect = std::dynamic_pointer_cast<PostProcessing::AutoExposureEffect>(
								effect
							);
							if (auto_exposure_effect) {
								float speed_up = auto_exposure_effect->GetSpeedUp();
								if (ImGui::SliderFloat("Speed Up", &speed_up, 0.1f, 10.0f)) {
									auto_exposure_effect->SetSpeedUp(speed_up);
								}
								float speed_down = auto_exposure_effect->GetSpeedDown();
								if (ImGui::SliderFloat("Speed Down", &speed_down, 0.1f, 10.0f)) {
									auto_exposure_effect->SetSpeedDown(speed_down);
								}
								float target_lum = auto_exposure_effect->GetTargetLuminance();
								if (ImGui::SliderFloat("Target Luminance", &target_lum, 0.01f, 1.0f)) {
									auto_exposure_effect->SetTargetLuminance(target_lum);
								}
								float min_exposure = auto_exposure_effect->GetMinExposure();
								if (ImGui::SliderFloat("Min Exposure", &min_exposure, 0.01f, 10.0f)) {
									auto_exposure_effect->SetMinExposure(min_exposure);
								}
								float max_exposure = auto_exposure_effect->GetMaxExposure();
								if (ImGui::SliderFloat("Max Exposure", &max_exposure, 1.0f, 100.0f)) {
									auto_exposure_effect->SetMaxExposure(max_exposure);
								}
							}
						}

						if (effect->GetName() == "Film Grain" && is_enabled) {
							auto film_grain_effect = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect);
							if (film_grain_effect) {
								float intensity = film_grain_effect->GetIntensity();
								if (ImGui::SliderFloat("Intensity##FilmGrain", &intensity, 0.0f, 1.0f)) {
									film_grain_effect->SetIntensity(intensity);
								}
							}
						}

						if (effect->GetName() == "GTAO" && is_enabled) {
							auto gtao_effect = std::dynamic_pointer_cast<PostProcessing::GtaoEffect>(effect);
							if (gtao_effect) {
								float radius = gtao_effect->GetRadius();
								if (ImGui::SliderFloat("Radius##GTAO", &radius, 0.01f, 5.0f)) {
									gtao_effect->SetRadius(radius);
								}
								float intensity = gtao_effect->GetIntensity();
								if (ImGui::SliderFloat("Intensity##GTAO", &intensity, 0.0f, 5.0f)) {
									gtao_effect->SetIntensity(intensity);
								}
								float ssdi_intensity = gtao_effect->GetSSDIIntensity();
								if (ImGui::SliderFloat("SSDI Intensity##GTAO", &ssdi_intensity, 0.0f, 5.0f)) {
									gtao_effect->SetSSDIIntensity(ssdi_intensity);
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

					if (auto effect = manager.GetToneMappingEffect()) {
						auto tone_mapping_effect = std::dynamic_pointer_cast<PostProcessing::ToneMappingEffect>(effect);
						ImGui::Separator();
						bool is_enabled = tone_mapping_effect->IsEnabled();
						if (ImGui::Checkbox("Tone Mapping", &is_enabled)) {
							tone_mapping_effect->SetEnabled(is_enabled);
						}
						if (is_enabled) {
							const char* modes[] = {
								"ACES",
								"Filmic",
								"Lottes",
								"Reinhard",
								"Reinhard II",
								"Uchimura",
								"Uncharted 2",
								"Unreal 3"
							};
							int current_mode = tone_mapping_effect->GetMode();
							if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
								tone_mapping_effect->SetMode(current_mode);
							}
						}
					}
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
