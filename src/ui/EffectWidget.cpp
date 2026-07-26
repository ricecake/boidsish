#include "ui/EffectWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/DepthOfFieldEffect.h"
#include "post_processing/effects/FXAAEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/UnifiedScreenSpaceEffect.h"
#include "post_processing/effects/ScreenSpaceWeatherEffect.h"

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
						// Atmosphere and Volumetric Lighting are handled in EnvironmentWidget
						if (effect->GetName() == "Atmosphere" || effect->GetName() == "Volumetric Lighting")
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

						if (effect->GetName() == "Screen Space Weather" && is_enabled) {
							auto weather_effect = std::dynamic_pointer_cast<PostProcessing::ScreenSpaceWeatherEffect>(effect);
							if (weather_effect) {
								if (ImGui::TreeNode("Heat Shimmer (Distortion)")) {
									float strength = weather_effect->GetHeatStrength();
									if (ImGui::SliderFloat("Strength##Heat", &strength, 0.0f, 2.0f)) {
										weather_effect->SetHeatStrength(strength);
									}
									float scale = weather_effect->GetHeatScale();
									if (ImGui::SliderFloat("Scale##Heat", &scale, 0.01f, 10.0f)) {
										weather_effect->SetHeatScale(scale);
									}
									float speed = weather_effect->GetHeatSpeed();
									if (ImGui::SliderFloat("Speed##Heat", &speed, 0.0f, 10.0f)) {
										weather_effect->SetHeatSpeed(speed);
									}
									ImGui::TreePop();
								}

								if (ImGui::TreeNode("Wind-driven Blur")) {
									float angle = weather_effect->GetWindAngle();
									if (ImGui::SliderFloat("Wind Angle (Degrees)##Wind", &angle, 0.0f, 360.0f)) {
										weather_effect->SetWindAngle(angle);
									}
									float speed = weather_effect->GetWindSpeed();
									if (ImGui::SliderFloat("Wind Speed (Base)##Wind", &speed, 0.0f, 20.0f)) {
										weather_effect->SetWindSpeed(speed);
									}
									float blur_scale = weather_effect->GetWindBlurScale();
									if (ImGui::SliderFloat("Blur Scale##Wind", &blur_scale, 0.0f, 0.1f, "%.4f")) {
										weather_effect->SetWindBlurScale(blur_scale);
									}
									float gust_freq = weather_effect->GetWindGustFrequency();
									if (ImGui::SliderFloat("Gust Frequency##Wind", &gust_freq, 0.01f, 10.0f)) {
										weather_effect->SetWindGustFrequency(gust_freq);
									}
									float gust_strength = weather_effect->GetWindGustStrength();
									if (ImGui::SliderFloat("Gust Strength##Wind", &gust_strength, 0.0f, 5.0f)) {
										weather_effect->SetWindGustStrength(gust_strength);
									}
									ImGui::TreePop();
								}

								if (ImGui::TreeNode("Frost & Ice Crystals")) {
									bool manual = weather_effect->IsManualIceCoverage();
									if (ImGui::Checkbox("Use Manual Coverage##Ice", &manual)) {
										weather_effect->SetManualIceCoverage(manual);
									}
									if (manual) {
										float coverage = weather_effect->GetIceCoverage();
										if (ImGui::SliderFloat("Coverage##Ice", &coverage, 0.0f, 1.0f)) {
											weather_effect->SetIceCoverage(coverage);
										}
									} else {
										ImGui::Text("Coverage driven by current temperature.");
									}
									float scale = weather_effect->GetIceScale();
									if (ImGui::SliderFloat("Crystal Scale##Ice", &scale, 1.0f, 30.0f)) {
										weather_effect->SetIceScale(scale);
									}
									float edge_width = weather_effect->GetIceEdgeWidth();
									if (ImGui::SliderFloat("Edge Width##Ice", &edge_width, 0.05f, 1.5f)) {
										weather_effect->SetIceEdgeWidth(edge_width);
									}
									glm::vec3 color = weather_effect->GetIceColor();
									if (ImGui::ColorEdit3("Ice Color##Ice", &color.x)) {
										weather_effect->SetIceColor(color);
									}
									ImGui::TreePop();
								}
							}
						}

						if (effect->GetName() == "Depth of Field" && is_enabled) {
							auto dof_effect = std::dynamic_pointer_cast<PostProcessing::DepthOfFieldEffect>(effect);
							if (dof_effect) {
								bool autofocus = dof_effect->IsAutofocusEnabled();
								if (ImGui::Checkbox("Autofocus##DoF", &autofocus)) {
									dof_effect->SetAutofocus(autofocus);
								}

								if (autofocus) {
									glm::vec2 offset = dof_effect->GetFocalPointOffset();
									if (ImGui::SliderFloat2("Focus Offset##DoF", &offset.x, -0.5f, 0.5f)) {
										dof_effect->SetFocalPointOffset(offset);
									}
									float minDist = dof_effect->GetMinFocusDistance();
									if (ImGui::SliderFloat("Min Distance##DoF", &minDist, 0.01f, 10.0f)) {
										dof_effect->SetMinFocusDistance(minDist);
									}
									float maxDist = dof_effect->GetMaxFocusDistance();
									if (ImGui::SliderFloat("Max Distance##DoF", &maxDist, 10.0f, 2000.0f)) {
										dof_effect->SetMaxFocusDistance(maxDist);
									}
								} else {
									float focusDist = dof_effect->GetFocusDistance();
									if (ImGui::SliderFloat("Focus Distance##DoF", &focusDist, 0.1f, 1000.0f)) {
										dof_effect->SetFocusDistance(focusDist);
									}
								}

								float focusScale = dof_effect->GetFocusScale();
								if (ImGui::SliderFloat("Focus Scale##DoF", &focusScale, 0.0f, 100.0f)) {
									dof_effect->SetFocusScale(focusScale);
								}
								float blurSize = dof_effect->GetBlurSize();
								if (ImGui::SliderFloat("Blur Size##DoF", &blurSize, 0.0f, 50.0f)) {
									dof_effect->SetBlurSize(blurSize);
								}
							}
						}

						if (effect->GetName() == "FXAA" && is_enabled) {
							auto fxaa_effect = std::dynamic_pointer_cast<PostProcessing::FXAAEffect>(effect);
							if (fxaa_effect) {
								float reduceMin = fxaa_effect->GetReduceMin();
								if (ImGui::SliderFloat("Reduce Min##FXAA", &reduceMin, 0.0f, 0.1f)) {
									fxaa_effect->SetReduceMin(reduceMin);
								}
								float reduceMul = fxaa_effect->GetReduceMul();
								if (ImGui::SliderFloat("Reduce Mul##FXAA", &reduceMul, 0.0f, 1.0f)) {
									fxaa_effect->SetReduceMul(reduceMul);
								}
								float spanMax = fxaa_effect->GetSpanMax();
								if (ImGui::SliderFloat("Span Max##FXAA", &spanMax, 0.0f, 20.0f)) {
									fxaa_effect->SetSpanMax(spanMax);
								}
								float lumaThreshold = fxaa_effect->GetLumaThreshold();
								if (ImGui::SliderFloat("Luma Threshold##FXAA", &lumaThreshold, 0.0f, 0.5f)) {
									fxaa_effect->SetLumaThreshold(lumaThreshold);
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
								bool bloom_enabled = bloom_effect->IsBloomEnabled();
								if (ImGui::Checkbox("Enable Bloom Blur", &bloom_enabled)) {
									bloom_effect->SetBloomEnabled(bloom_enabled);
								}

								if (bloom_enabled) {
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
								}

								if (ImGui::BeginTabBar("BloomTabs")) {
									auto drawSettings = [&](const char* label, PostProcessing::BloomEffect::LayerSettings& settings, bool isScene) {
										if (ImGui::BeginTabItem(label)) {
											ImGui::Text("Auto-Exposure");
											ImGui::Checkbox("Enable AE", &settings.autoExposureEnabled);
											if (settings.autoExposureEnabled) {
												ImGui::SliderFloat("Screen Brightness (Key Value)", &settings.targetLuminance, 0.01f, 1.0f);
												ImGui::SliderFloat("Speed Up", &settings.speedUp, 0.1f, 10.0f);
												ImGui::SliderFloat("Speed Down", &settings.speedDown, 0.1f, 10.0f);
												ImGui::SliderFloat("Min EV100", &settings.minEV100, -20.0f, 20.0f, "%.1f");
												ImGui::SliderFloat("Max EV100", &settings.maxEV100, -20.0f, 20.0f, "%.1f");
												ImGui::SliderFloat("Center Weight", &settings.centerWeightTightness, 0.0f, 10.0f);
												ImGui::SliderFloat2("Focus Point", &settings.focusPoint.x, 0.0f, 1.0f);
												ImGui::SliderFloat("Histogram Low Cutoff", &settings.histogramLowCutoff, 0.0f, 1.0f);
												ImGui::SliderFloat("Histogram High Cutoff", &settings.histogramHighCutoff, 0.0f, 1.0f);
											} else {
												ImGui::SliderFloat("Exposure Time (s)", &settings.exposureTime, 0.0001f, 1.0f, "%.4f");
												ImGui::SliderFloat("ISO", &settings.iso, 10.0f, 6400.0f, "%.0f");
												ImGui::SliderFloat("Aperture (f-stop)", &settings.aperture, 1.0f, 22.0f, "%.1f");
											}
											ImGui::Separator();
											ImGui::Checkbox("Tone Mapping", &settings.toneMappingEnabled);
											if (settings.toneMappingEnabled) {
												const char* modes[] = { "ACES", "Filmic", "Lottes", "Reinhard", "Reinhard II", "Uchimura", "Uncharted 2", "Unreal 3", "Debug" };
												ImGui::Combo("Mode", &settings.toneMappingMode, modes, IM_ARRAYSIZE(modes));

												if (settings.toneMappingMode == 5) { // Uchimura
													ImGui::SliderFloat("Max Brightness (P)", &settings.uchimuraP, 0.1f, 10.0f);
													ImGui::SliderFloat("Contrast (a)", &settings.uchimuraA, 0.1f, 5.0f);
													ImGui::SliderFloat("Linear Start (m)", &settings.uchimuraM, 0.0f, 1.0f);
													ImGui::SliderFloat("Linear Length (l)", &settings.uchimuraL, 0.0f, 1.0f);
													ImGui::SliderFloat("Black (c)", &settings.uchimuraC, 1.0f, 5.0f);
													ImGui::SliderFloat("Pedestal (b)", &settings.uchimuraB, 0.0f, 1.0f);
												}
											}

											ImGui::SliderFloat("Gamma", &settings.gamma, 1.0f, 3.0f, "%.2f");

											if (ImGui::TreeNode("Color Pipeline")) {
												ImGui::Checkbox("Enable Auto-tune", &settings.autoTuneEnabled);
												if (settings.autoTuneEnabled) {
													ImGui::SliderFloat("Min Contrast", &settings.minContrast, 0.1f, 1.0f);
													ImGui::SliderFloat("Max Contrast", &settings.maxContrast, 1.0f, 5.0f);
													ImGui::SliderFloat("Target Brightness", &settings.targetBrightness, 0.1f, 2.0f);
												}

												ImGui::Separator();
												ImGui::Text("ASC CDL");
												ImGui::ColorEdit3("Slope", &settings.cdlSlope.x);
												ImGui::ColorEdit3("Offset", &settings.cdlOffset.x);
												ImGui::ColorEdit3("Power", &settings.cdlPower.x);
												ImGui::SliderFloat("Saturation", &settings.cdlSaturation, 0.0f, 2.0f);

												ImGui::Separator();
												ImGui::Text("White Balance");
												ImGui::SliderFloat("Temperature (K)", &settings.whiteTemp, 2000.0f, 12000.0f);
												ImGui::SliderFloat("Tint", &settings.whiteTint, -1.0f, 1.0f);

												ImGui::Separator();
												ImGui::Text("Local Tone Mapping (Exposure Fusion)");
												ImGui::Checkbox("Enable LTM", &settings.ltmEnabled);
												if (settings.ltmEnabled) {
													ImGui::SliderFloat("EV Spread", &settings.ltmEvSpread, 0.0f, 4.0f);
													ImGui::SliderFloat("Well Exposed Target", &settings.ltmTarget, 0.0f, 1.0f);
													ImGui::SliderFloat("Well Exposed Sigma", &settings.ltmSigma, 0.01f, 1.0f);
													ImGui::SliderFloat("Weight Contrast", &settings.ltmWeightContrast, 0.0f, 1.0f);
													ImGui::SliderFloat("Weight Saturation", &settings.ltmWeightSaturation, 0.0f, 1.0f);
													ImGui::SliderFloat("Weight Exposedness", &settings.ltmWeightExposedness, 0.0f, 1.0f);
													ImGui::SliderFloat("Boost Local Contrast", &settings.ltmBoostLocalContrast, 0.0f, 2.0f);
												}
												ImGui::TreePop();
											}
											ImGui::EndTabItem();
										}
									};

									drawSettings("Scene", bloom_effect->GetSceneSettings(), true);
									drawSettings("Sky", bloom_effect->GetSkySettings(), false);

									ImGui::EndTabBar();
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
