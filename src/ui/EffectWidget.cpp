#include "ui/EffectWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/UnifiedScreenSpaceEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"
#include "post_processing/effects/VolumetricLightingEffect.h"

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

						if (effect->GetName() == "UnifiedScreenSpace" && is_enabled) {
							auto unified = std::dynamic_pointer_cast<PostProcessing::UnifiedScreenSpaceEffect>(effect);
							if (unified) {
								const char* res_modes[] = { "Full", "1/2", "1/4" };
								int current_res = 0;
								if (unified->GetResolutionScale() == PostProcessing::ScreenSpaceResolution::Half) current_res = 1;
								else if (unified->GetResolutionScale() == PostProcessing::ScreenSpaceResolution::Quarter) current_res = 2;

								if (ImGui::Combo("Resolution Scale##Unified", &current_res, res_modes, IM_ARRAYSIZE(res_modes))) {
									if (current_res == 0) unified->SetResolutionScale(PostProcessing::ScreenSpaceResolution::Full);
									else if (current_res == 1) unified->SetResolutionScale(PostProcessing::ScreenSpaceResolution::Half);
									else if (current_res == 2) unified->SetResolutionScale(PostProcessing::ScreenSpaceResolution::Quarter);
								}

								if (ImGui::TreeNode("SSGI")) {
									bool ssgi_enabled = unified->IsSSGIEnabled();
									if (ImGui::Checkbox("Enabled##SSGI", &ssgi_enabled)) unified->SetSSGIEnabled(ssgi_enabled);

									float intensity = unified->GetSSGIIntensity();
									if (ImGui::SliderFloat("Intensity##SSGI", &intensity, 0.0f, 5.0f)) unified->SetSSGIIntensity(intensity);

									float radius = unified->GetSSGIRadius();
									if (ImGui::SliderFloat("Radius##SSGI", &radius, 0.01f, 10.0f)) unified->SetSSGIRadius(radius);

									float falloff = unified->GetSSGIDistanceFalloff();
									if (ImGui::SliderFloat("Falloff##SSGI", &falloff, 0.01f, 5.0f)) unified->SetSSGIDistanceFalloff(falloff);

									int steps = unified->GetSSGISteps();
									if (ImGui::SliderInt("Steps##SSGI", &steps, 1, 32)) unified->SetSSGISteps(steps);

									int rays = unified->GetSSGIRayCount();
									if (ImGui::SliderInt("Rays##SSGI", &rays, 1, 8)) unified->SetSSGIRayCount(rays);

									float refl_intensity = unified->GetSSGIReflectionIntensity();
									if (ImGui::SliderFloat("Reflection Intensity##SSGI", &refl_intensity, 0.0f, 5.0f)) unified->SetSSGIReflectionIntensity(refl_intensity);

									float rough_factor = unified->GetSSGIRoughnessFactor();
									if (ImGui::SliderFloat("Roughness Factor##SSGI", &rough_factor, 0.1f, 2.0f)) unified->SetSSGIRoughnessFactor(rough_factor);

									ImGui::TreePop();
								}

								if (ImGui::TreeNode("GTAO")) {
									bool gtao_enabled = unified->IsGTAOEnabled();
									if (ImGui::Checkbox("Enabled##GTAO", &gtao_enabled)) unified->SetGTAOEnabled(gtao_enabled);

									float radius = unified->GetGTAORadius();
									if (ImGui::SliderFloat("Radius##GTAO", &radius, 0.01f, 5.0f)) unified->SetGTAORadius(radius);

									float intensity = unified->GetGTAOIntensity();
									if (ImGui::SliderFloat("Intensity##GTAO", &intensity, 0.0f, 5.0f)) unified->SetGTAOIntensity(intensity);

									float falloff = unified->GetGTAOFalloff();
									if (ImGui::SliderFloat("Falloff##GTAO", &falloff, 0.01f, 5.0f)) unified->SetGTAOFalloff(falloff);

									int steps = unified->GetGTAOSteps();
									if (ImGui::SliderInt("Steps##GTAO", &steps, 1, 32)) unified->SetGTAOSteps(steps);

									int dirs = unified->GetGTAODirections();
									if (ImGui::SliderInt("Directions##GTAO", &dirs, 1, 8)) unified->SetGTAODirections(dirs);

									ImGui::TreePop();
								}

								if (ImGui::TreeNode("SSS")) {
									bool sss_enabled = unified->IsSSSEnabled();
									if (ImGui::Checkbox("Enabled##SSS", &sss_enabled)) unified->SetSSSEnabled(sss_enabled);

									float intensity = unified->GetSSSIntensity();
									if (ImGui::SliderFloat("Intensity##SSS", &intensity, 0.0f, 1.0f)) unified->SetSSSIntensity(intensity);

									float radius = unified->GetSSSRadius();
									if (ImGui::SliderFloat("Radius##SSS", &radius, 0.01f, 5.0f)) unified->SetSSSRadius(radius);

									float bias = unified->GetSSSBias();
									if (ImGui::SliderFloat("Bias##SSS", &bias, 0.001f, 0.5f)) unified->SetSSSBias(bias);

									int steps = unified->GetSSSSteps();
									if (ImGui::SliderInt("Steps##SSS", &steps, 4, 64)) unified->SetSSSSteps(steps);

									ImGui::TreePop();
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

						if (effect->GetName() == "Volumetric Lighting" && is_enabled) {
							auto vol_effect = std::dynamic_pointer_cast<PostProcessing::VolumetricLightingEffect>(
								effect
							);
							if (vol_effect) {
								float scat = vol_effect->GetScatteringCoefficient();
								if (ImGui::SliderFloat("Scattering##Vol", &scat, 0.0f, 1.0f)) {
									vol_effect->SetScatteringCoefficient(scat);
								}
								float abs = vol_effect->GetAbsorptionCoefficient();
								if (ImGui::SliderFloat("Absorption##Vol", &abs, 0.0f, 1.0f)) {
									vol_effect->SetAbsorptionCoefficient(abs);
								}
								float phase = vol_effect->GetPhaseG();
								if (ImGui::SliderFloat("Phase G##Vol", &phase, -0.99f, 0.99f)) {
									vol_effect->SetPhaseG(phase);
								}
								float dens = vol_effect->GetHazeDensity();
								if (ImGui::SliderFloat("Haze Density##Vol", &dens, 0.0f, 20.0f)) {
									vol_effect->SetHazeParams(dens, vol_effect->GetHazeHeight());
								}
								float intens = vol_effect->GetIntensity();
								if (ImGui::SliderFloat("Intensity##Vol", &intens, 0.0f, 500.0f)) {
									vol_effect->SetIntensity(intens);
								}

								int technique = static_cast<int>(vol_effect->GetTechnique());
								const char* techniques[] = {"Grid (Robust)", "Epipolar (Optimized)"};
								if (ImGui::Combo("Technique##Vol", &technique, techniques, 2)) {
									vol_effect->SetTechnique(static_cast<PostProcessing::VolumetricTechnique>(technique));
								}

								ImGui::Separator();
								ImGui::Text("Light Contributions");
								auto& lights = m_visualizer.GetLightManager().GetLights();
								for (size_t i = 0; i < lights.size(); ++i) {
									if (!lights[i].casts_shadow)
										continue;
									std::string label = "Light " + std::to_string(i) + "##VolContrib";
									ImGui::SliderFloat(
										label.c_str(),
										&lights[i].volumetric_intensity,
										0.0f,
										20.0f
									);
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
