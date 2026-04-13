#include "ui/EffectWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/GtaoEffect.h"
#include "post_processing/effects/ScreenSpaceShadowEffect.h"
#include "post_processing/effects/SsgiEffect.h"
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

						if (effect->GetName() == "SSGI" && is_enabled) {
							auto ssgi_effect = std::dynamic_pointer_cast<PostProcessing::SsgiEffect>(effect);
							if (ssgi_effect) {
								float intensity = ssgi_effect->GetIntensity();
								if (ImGui::SliderFloat("Intensity##SSGI", &intensity, 0.0f, 5.0f)) {
									ssgi_effect->SetIntensity(intensity);
								}
								float radius = ssgi_effect->GetRadius();
								if (ImGui::SliderFloat("Radius##SSGI", &radius, 0.01f, 10.0f)) {
									ssgi_effect->SetRadius(radius);
								}
								float falloff = ssgi_effect->GetDistanceFalloff();
								if (ImGui::SliderFloat("Falloff##SSGI", &falloff, 0.01f, 5.0f)) {
									ssgi_effect->SetDistanceFalloff(falloff);
								}
								int steps = ssgi_effect->GetSteps();
								if (ImGui::SliderInt("Steps##SSGI", &steps, 1, 32)) {
									ssgi_effect->SetSteps(steps);
								}
								int rays = ssgi_effect->GetRayCount();
								if (ImGui::SliderInt("Rays##SSGI", &rays, 1, 8)) {
									ssgi_effect->SetRayCount(rays);
								}
								float refl_intensity = ssgi_effect->GetReflectionIntensity();
								if (ImGui::SliderFloat("Reflection Intensity##SSGI", &refl_intensity, 0.0f, 5.0f)) {
									ssgi_effect->SetReflectionIntensity(refl_intensity);
								}
								float rough_factor = ssgi_effect->GetRoughnessFactor();
								if (ImGui::SliderFloat("Roughness Factor##SSGI", &rough_factor, 0.1f, 2.0f)) {
									ssgi_effect->SetRoughnessFactor(rough_factor);
								}
							}
						}

						if (effect->GetName() == "ScreenSpaceShadows" && is_enabled) {
							auto sss_effect = std::dynamic_pointer_cast<PostProcessing::ScreenSpaceShadowEffect>(effect);
							if (sss_effect) {
								float intensity = sss_effect->GetIntensity();
								if (ImGui::SliderFloat("Intensity##SSS", &intensity, 0.0f, 1.0f)) {
									sss_effect->SetIntensity(intensity);
								}
								float radius = sss_effect->GetRadius();
								if (ImGui::SliderFloat("Radius##SSS", &radius, 0.01f, 5.0f)) {
									sss_effect->SetRadius(radius);
								}
								float bias = sss_effect->GetBias();
								if (ImGui::SliderFloat("Bias##SSS", &bias, 0.001f, 0.5f)) {
									sss_effect->SetBias(bias);
								}
								int steps = sss_effect->GetSteps();
								if (ImGui::SliderInt("Steps##SSS", &steps, 4, 64)) {
									sss_effect->SetSteps(steps);
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
								float min_intensity = bloom_effect->GetMinIntensity();
								if (ImGui::SliderFloat("Min Intensity (AE)", &min_intensity, 0.0f, 5.0f)) {
									bloom_effect->SetMinIntensity(min_intensity);
								}
								float max_intensity = bloom_effect->GetMaxIntensity();
								if (ImGui::SliderFloat("Max Intensity (AE)", &max_intensity, 0.0f, 10.0f)) {
									bloom_effect->SetMaxIntensity(max_intensity);
								}

								ImGui::Separator();
								ImGui::Text("Auto-Exposure");
								bool ae_enabled = bloom_effect->IsAutoExposureEnabled();
								if (ImGui::Checkbox("Enable AE", &ae_enabled)) {
									bloom_effect->SetAutoExposureEnabled(ae_enabled);
								}
								if (ae_enabled) {
									float target_lum = bloom_effect->GetTargetLuminance();
									if (ImGui::SliderFloat("Target Luminance", &target_lum, 0.01f, 1.0f)) {
										bloom_effect->SetTargetLuminance(target_lum);
									}
									float speed_up = bloom_effect->GetSpeedUp();
									if (ImGui::SliderFloat("Speed Up", &speed_up, 0.1f, 10.0f)) {
										bloom_effect->SetAdaptationSpeeds(speed_up, bloom_effect->GetSpeedDown());
									}
									float speed_down = bloom_effect->GetSpeedDown();
									if (ImGui::SliderFloat("Speed Down", &speed_down, 0.1f, 10.0f)) {
										bloom_effect->SetAdaptationSpeeds(bloom_effect->GetSpeedUp(), speed_down);
									}
									float min_exposure = bloom_effect->GetMinExposure();
									if (ImGui::SliderFloat("Min Exposure", &min_exposure, 0.01f, 10.0f)) {
										bloom_effect->SetExposureLimits(min_exposure, bloom_effect->GetMaxExposure());
									}
									float max_exposure = bloom_effect->GetMaxExposure();
									if (ImGui::SliderFloat("Max Exposure", &max_exposure, 1.0f, 100.0f)) {
										bloom_effect->SetExposureLimits(bloom_effect->GetMinExposure(), max_exposure);
									}
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
