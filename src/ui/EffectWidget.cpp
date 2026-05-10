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

		void DrawLayerSettings(PostProcessing::LayerSettings& settings, const char* suffix) {
			ImGui::Text("Auto-Exposure");
			if (ImGui::Checkbox((std::string("Enable AE##") + suffix).c_str(), &settings.autoExposureEnabled)) {}

			if (settings.autoExposureEnabled) {
				ImGui::SliderFloat((std::string("Target Luminance##") + suffix).c_str(), &settings.targetLuminance, 0.01f, 1.0f);
				ImGui::SliderFloat((std::string("Speed Up##") + suffix).c_str(), &settings.speedUp, 0.1f, 10.0f);
				ImGui::SliderFloat((std::string("Speed Down##") + suffix).c_str(), &settings.speedDown, 0.1f, 10.0f);
				ImGui::SliderFloat((std::string("Min Exposure##") + suffix).c_str(), &settings.minExposure, 0.01f, 10.0f);
				ImGui::SliderFloat((std::string("Max Exposure##") + suffix).c_str(), &settings.maxExposure, 1.0f, 100.0f);
				ImGui::SliderFloat((std::string("Center Weight##") + suffix).c_str(), &settings.centerWeightTightness, 0.0f, 10.0f);
				ImGui::SliderFloat2((std::string("Focus Point##") + suffix).c_str(), &settings.focusPoint.x, 0.0f, 1.0f);
				ImGui::SliderFloat((std::string("Histogram Low Cutoff##") + suffix).c_str(), &settings.histogramLowCutoff, 0.0f, 1.0f);
				ImGui::SliderFloat((std::string("Histogram High Cutoff##") + suffix).c_str(), &settings.histogramHighCutoff, 0.0f, 1.0f);
			}

			ImGui::Separator();
			ImGui::Checkbox((std::string("Tone Mapping##") + suffix).c_str(), &settings.toneMappingEnabled);
			if (settings.toneMappingEnabled) {
				const char* modes[] = { "ACES", "Filmic", "Lottes", "Reinhard", "Reinhard II", "Uchimura", "Uncharted 2", "Unreal 3", "Debug" };
				ImGui::Combo((std::string("Mode##") + suffix).c_str(), &settings.toneMappingMode, modes, IM_ARRAYSIZE(modes));

				if (settings.toneMappingMode == 5) { // Uchimura
					ImGui::SliderFloat((std::string("Max Brightness (P)##") + suffix).c_str(), &settings.uchimuraP, 0.1f, 10.0f);
					ImGui::SliderFloat((std::string("Contrast (a)##") + suffix).c_str(), &settings.uchimuraA, 0.1f, 5.0f);
					ImGui::SliderFloat((std::string("Linear Start (m)##") + suffix).c_str(), &settings.uchimuraM, 0.0f, 1.0f);
					ImGui::SliderFloat((std::string("Linear Length (l)##") + suffix).c_str(), &settings.uchimuraL, 0.0f, 1.0f);
					ImGui::SliderFloat((std::string("Black (c)##") + suffix).c_str(), &settings.uchimuraC, 1.0f, 5.0f);
					ImGui::SliderFloat((std::string("Pedestal (b)##") + suffix).c_str(), &settings.uchimuraB, 0.0f, 1.0f);
				}
			}

			if (ImGui::TreeNode((std::string("Color Pipeline##") + suffix).c_str())) {
				ImGui::Checkbox((std::string("Enable Auto-tune##") + suffix).c_str(), &settings.autoTuneEnabled);
				if (settings.autoTuneEnabled) {
					ImGui::SliderFloat((std::string("Min Contrast##") + suffix).c_str(), &settings.minContrast, 0.1f, 1.0f);
					ImGui::SliderFloat((std::string("Max Contrast##") + suffix).c_str(), &settings.maxContrast, 1.0f, 5.0f);
					ImGui::SliderFloat((std::string("Target Brightness##") + suffix).c_str(), &settings.targetBrightness, 0.1f, 2.0f);
				}

				ImGui::Separator();
				ImGui::Text("ASC CDL");
				ImGui::ColorEdit3((std::string("Slope##") + suffix).c_str(), &settings.cdlSlope.x);
				ImGui::ColorEdit3((std::string("Offset##") + suffix).c_str(), &settings.cdlOffset.x);
				ImGui::ColorEdit3((std::string("Power##") + suffix).c_str(), &settings.cdlPower.x);
				ImGui::SliderFloat((std::string("Saturation##") + suffix).c_str(), &settings.cdlSaturation, 0.0f, 2.0f);

				ImGui::Separator();
				ImGui::Text("White Balance");
				ImGui::SliderFloat((std::string("Temperature (K)##") + suffix).c_str(), &settings.whiteTemp, 2000.0f, 12000.0f);
				ImGui::SliderFloat((std::string("Tint##") + suffix).c_str(), &settings.whiteTint, -1.0f, 1.0f);

				ImGui::Separator();
				ImGui::Text("Local Tone Mapping (Exposure Fusion)");
				ImGui::Checkbox((std::string("Enable LTM##") + suffix).c_str(), &settings.ltmEnabled);
				if (settings.ltmEnabled) {
					ImGui::SliderFloat((std::string("EV Spread##") + suffix).c_str(), &settings.ltmEvSpread, 0.0f, 4.0f);
					ImGui::SliderFloat((std::string("Well Exposed Target##") + suffix).c_str(), &settings.ltmTarget, 0.0f, 1.0f);
					ImGui::SliderFloat((std::string("Well Exposed Sigma##") + suffix).c_str(), &settings.ltmSigma, 0.01f, 1.0f);
					ImGui::SliderFloat((std::string("Weight Contrast##") + suffix).c_str(), &settings.ltmWeightContrast, 0.0f, 1.0f);
					ImGui::SliderFloat((std::string("Weight Saturation##") + suffix).c_str(), &settings.ltmWeightSaturation, 0.0f, 1.0f);
					ImGui::SliderFloat((std::string("Weight Exposedness##") + suffix).c_str(), &settings.ltmWeightExposedness, 0.0f, 1.0f);
					ImGui::SliderFloat((std::string("Boost Local Contrast##") + suffix).c_str(), &settings.ltmBoostLocalContrast, 0.0f, 2.0f);
				}
				ImGui::TreePop();
			}
		}

		void EffectWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(540, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Effects", &m_show)) {
				// 1. Artistic Effects
				if (ImGui::CollapsingHeader("Artistic Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();

					bool ripple_enabled = config.GetAppSettingBool("artistic_effect_ripple", false);
					if (ImGui::Checkbox("Ripple", &ripple_enabled)) config.SetBool("artistic_effect_ripple", ripple_enabled);

					bool color_shift_enabled = config.GetAppSettingBool("artistic_effect_color_shift", false);
					if (ImGui::Checkbox("Color Shift", &color_shift_enabled)) config.SetBool("artistic_effect_color_shift", color_shift_enabled);

					bool bnw_enabled = config.GetAppSettingBool("artistic_effect_black_and_white", false);
					if (ImGui::Checkbox("Black and White", &bnw_enabled)) config.SetBool("artistic_effect_black_and_white", bnw_enabled);

					bool negative_enabled = config.GetAppSettingBool("artistic_effect_negative", false);
					if (ImGui::Checkbox("Negative (Artistic)", &negative_enabled)) config.SetBool("artistic_effect_negative", negative_enabled);

					bool shimmery_enabled = config.GetAppSettingBool("artistic_effect_shimmery", false);
					if (ImGui::Checkbox("Shimmery", &shimmery_enabled)) config.SetBool("artistic_effect_shimmery", shimmery_enabled);

					bool glitched_enabled = config.GetAppSettingBool("artistic_effect_glitched", false);
					if (ImGui::Checkbox("Glitched", &glitched_enabled)) config.SetBool("artistic_effect_glitched", glitched_enabled);

					bool wireframe_enabled = config.GetAppSettingBool("artistic_effect_wireframe", false);
					if (ImGui::Checkbox("Wireframe", &wireframe_enabled)) config.SetBool("artistic_effect_wireframe", wireframe_enabled);
				}

				// 2. Post-Processing Effects
				if (ImGui::CollapsingHeader("Post-Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& manager = m_visualizer.GetPostProcessingManager();

					for (auto& effect : manager.GetPreToneMappingEffects()) {
						if (effect->GetName() == "Atmosphere") continue;

						bool is_enabled = effect->IsEnabled();
						if (ImGui::Checkbox(effect->GetName().c_str(), &is_enabled)) effect->SetEnabled(is_enabled);

						if (effect->GetName() == "Film Grain" && is_enabled) {
							auto film_grain_effect = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect);
							if (film_grain_effect) {
								float intensity = film_grain_effect->GetIntensity();
								if (ImGui::SliderFloat("Intensity##FilmGrain", &intensity, 0.0f, 1.0f)) film_grain_effect->SetIntensity(intensity);
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
								if (ImGui::SliderFloat("Intensity##Bloom", &intensity, 0.0f, 2.0f)) bloom_effect->SetIntensity(intensity);
								float threshold = bloom_effect->GetThreshold();
								if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 3.0f)) bloom_effect->SetThreshold(threshold);
								float min_intensity = bloom_effect->GetMinIntensity();
								if (ImGui::SliderFloat("Min Intensity (AE)", &min_intensity, 0.0f, 5.0f)) bloom_effect->SetMinIntensity(min_intensity);
								float max_intensity = bloom_effect->GetMaxIntensity();
								if (ImGui::SliderFloat("Max Intensity (AE)", &max_intensity, 0.0f, 10.0f)) bloom_effect->SetMaxIntensity(max_intensity);

								ImGui::Separator();
								if (ImGui::TreeNode("Scene Exposure & Tonemapping")) {
									DrawLayerSettings(bloom_effect->GetSceneSettings(), "scene");
									ImGui::TreePop();
								}
								if (ImGui::TreeNode("Sky Exposure & Tonemapping")) {
									DrawLayerSettings(bloom_effect->GetSkySettings(), "sky");
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
