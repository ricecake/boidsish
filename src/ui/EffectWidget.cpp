#include "ui/EffectWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/UnifiedScreenSpaceEffect.h"

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

									float center_weight = bloom_effect->GetAutoExposureCenterWeight();
									if (ImGui::SliderFloat("Center Weight", &center_weight, 0.0f, 10.0f)) {
										bloom_effect->SetAutoExposureCenterWeight(center_weight);
									}

									glm::vec2 focus_point = bloom_effect->GetAutoExposureFocusPoint();
									if (ImGui::SliderFloat2("Focus Point", &focus_point.x, 0.0f, 1.0f)) {
										bloom_effect->SetAutoExposureFocusPoint(focus_point);
									}

									float low_cutoff = bloom_effect->GetHistogramLowCutoff();
									float high_cutoff = bloom_effect->GetHistogramHighCutoff();
									if (ImGui::SliderFloat("Histogram Low Cutoff", &low_cutoff, 0.0f, 1.0f)) {
										bloom_effect->SetHistogramCutoffs(low_cutoff, high_cutoff);
									}
									if (ImGui::SliderFloat("Histogram High Cutoff", &high_cutoff, 0.0f, 1.0f)) {
										bloom_effect->SetHistogramCutoffs(low_cutoff, high_cutoff);
									}

									ImGui::Separator();
									ImGui::Text("Local Tone Mapping (Fusion)");
									bool ltm_enabled = bloom_effect->IsLtmEnabled();
									if (ImGui::Checkbox("Enable LTM", &ltm_enabled)) {
										bloom_effect->SetLtmEnabled(ltm_enabled);
									}
									if (ltm_enabled) {
										float shadows = bloom_effect->GetLtmShadows();
										float highlights = bloom_effect->GetLtmHighlights();
										float sigma = bloom_effect->GetLtmSigma();
										bool changed_ltm = false;
										changed_ltm |= ImGui::SliderFloat("Shadows Boost", &shadows, 0.0f, 4.0f);
										changed_ltm |= ImGui::SliderFloat("Highlights Compression", &highlights, 0.0f, 4.0f);
										changed_ltm |= ImGui::SliderFloat("Exposure Sigma", &sigma, 0.1f, 10.0f);
										if (changed_ltm) {
											bloom_effect->SetLtmParams(shadows, highlights, sigma);
										}
										bool boost = bloom_effect->IsLtmBoostContrastEnabled();
										if (ImGui::Checkbox("Boost Local Contrast", &boost)) {
											bloom_effect->SetLtmBoostContrast(boost);
										}
										int max_mip = bloom_effect->GetLtmMaxMip();
										if (ImGui::SliderInt("Fusion Depth (Mip)", &max_mip, 0, 5)) {
											bloom_effect->SetLtmMaxMip(max_mip);
										}
									}
								}
								ImGui::Separator();
								bool is_enabled = bloom_effect->IsToneMappingEnabled();
								if (ImGui::Checkbox("Tone Mapping", &is_enabled)) {
									bloom_effect->SetToneMappingEnabled(is_enabled);
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
										"Unreal 3",
										"Debug",
									};
									int current_mode = bloom_effect->GetToneMappingMode();
									if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
										bloom_effect->SetToneMappingMode(current_mode);
									}

									if (current_mode == 5) { // Uchimura
										float P = bloom_effect->GetUchimuraP();
										float a = bloom_effect->GetUchimuraA();
										float m = bloom_effect->GetUchimuraM();
										float l = bloom_effect->GetUchimuraL();
										float c = bloom_effect->GetUchimuraC();
										float b = bloom_effect->GetUchimuraB();

										bool changed = false;
										changed |= ImGui::SliderFloat("Max Brightness (P)", &P, 0.1f, 10.0f);
										changed |= ImGui::SliderFloat("Contrast (a)", &a, 0.1f, 5.0f);
										changed |= ImGui::SliderFloat("Linear Start (m)", &m, 0.0f, 1.0f);
										changed |= ImGui::SliderFloat("Linear Length (l)", &l, 0.0f, 1.0f);
										changed |= ImGui::SliderFloat("Black (c)", &c, 1.0f, 5.0f);
										changed |= ImGui::SliderFloat("Pedestal (b)", &b, 0.0f, 1.0f);

										if (changed) {
											bloom_effect->SetUchimuraParams(P, a, m, l, c, b);
										}
									}
								}

								if (ImGui::TreeNode("Color Pipeline")) {
									ImGui::Text("Auto-tune Tonemapper");
									bool auto_tune = bloom_effect->IsAutoTuneEnabled();
									if (ImGui::Checkbox("Enable Auto-tune", &auto_tune)) {
										bloom_effect->SetAutoTuneEnabled(auto_tune);
									}
									if (auto_tune) {
										float min_c = bloom_effect->GetMinContrast();
										float max_c = bloom_effect->GetMaxContrast();
										float target_b = bloom_effect->GetTargetBrightness();
										bool changed_at = false;
										changed_at |= ImGui::SliderFloat("Min Contrast", &min_c, 0.1f, 1.0f);
										changed_at |= ImGui::SliderFloat("Max Contrast", &max_c, 1.0f, 5.0f);
										changed_at |= ImGui::SliderFloat("Target Brightness", &target_b, 0.1f, 2.0f);
										if (changed_at) {
											bloom_effect->SetAutoTuneConstraints(min_c, max_c, target_b);
										}
									}

									ImGui::Separator();
									ImGui::Text("ASC CDL");
									glm::vec3 slope = bloom_effect->GetCdlSlope();
									glm::vec3 offset = bloom_effect->GetCdlOffset();
									glm::vec3 power = bloom_effect->GetCdlPower();
									float saturation = bloom_effect->GetCdlSaturation();

									bool changed_cdl = false;
									changed_cdl |= ImGui::ColorEdit3("Slope", &slope.x);
									changed_cdl |= ImGui::ColorEdit3("Offset", &offset.x);
									changed_cdl |= ImGui::ColorEdit3("Power", &power.x);
									changed_cdl |= ImGui::SliderFloat("Saturation", &saturation, 0.0f, 2.0f);
									if (changed_cdl) {
										bloom_effect->SetCdlParams(slope, offset, power, saturation);
									}

									ImGui::Separator();
									ImGui::Text("White Balance");
									float temp = bloom_effect->GetWhiteTemp();
									float tint = bloom_effect->GetWhiteTint();
									bool changed_wb = false;
									changed_wb |= ImGui::SliderFloat("Temperature (K)", &temp, 2000.0f, 12000.0f);
									changed_wb |= ImGui::SliderFloat("Tint", &tint, -1.0f, 1.0f);
									if (changed_wb) {
										bloom_effect->SetWhiteBalance(temp, tint);
									}
									ImGui::TreePop();
								}
							}
						}
					}
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
