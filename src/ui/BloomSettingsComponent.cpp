#include "ui/BloomSettingsComponent.h"

#include <memory>
#include <string>
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/BloomEffect.h"

namespace Boidsish {
	namespace UI {

		void BloomSettingsComponent::Draw(Visualizer& visualizer) {
			auto& manager = visualizer.GetPostProcessingManager();
			std::shared_ptr<PostProcessing::BloomEffect> bloom_effect = nullptr;
			for (auto& effect : manager.GetPreToneMappingEffects()) {
				if (effect->GetName() == "Bloom") {
					bloom_effect = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect);
					break;
				}
			}

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

								if (isScene) {
									ImGui::Separator();
									ImGui::Text("Depth-Based Grading Layers");
									auto& additionalLayers = bloom_effect->GetAdditionalCdlEntries();
									if (ImGui::Button("Add Layer##CDL")) {
										PostProcessing::BloomEffect::CdlGradingEntry newEntry;
										bloom_effect->AddCdlEntry(newEntry);
									}

									for (size_t i = 0; i < additionalLayers.size(); ++i) {
										ImGui::PushID(static_cast<int>(i));
										std::string headerName = "Layer " + std::to_string(i + 1);
										if (ImGui::TreeNode(headerName.c_str())) {
											auto& entry = additionalLayers[i];
											ImGui::Checkbox("Enabled##CdlLayer", &entry.enabled);
											ImGui::SliderFloat("Target Depth##CdlLayer", &entry.targetDepth, 0.1f, 1000.0f);
											ImGui::SliderFloat("Falloff Width##CdlLayer", &entry.falloffWidth, 0.1f, 500.0f);
											ImGui::SliderFloat("Falloff Rate##CdlLayer", &entry.falloffRate, 0.1f, 10.0f);
											ImGui::InputInt("Priority##CdlLayer", &entry.priority);

											ImGui::ColorEdit3("Slope##CdlLayer", &entry.cdlSlope.x);
											ImGui::ColorEdit3("Offset##CdlLayer", &entry.cdlOffset.x);
											ImGui::ColorEdit3("Power##CdlLayer", &entry.cdlPower.x);
											ImGui::SliderFloat("Saturation##CdlLayer", &entry.cdlSaturation, 0.0f, 2.0f);

											if (ImGui::Button("Remove Layer##CdlLayer")) {
												bloom_effect->RemoveCdlEntry(i);
												ImGui::TreePop();
												ImGui::PopID();
												break; // break the loop as vector size changed
											}
											ImGui::TreePop();
										}
										ImGui::PopID();
									}
								}

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
			} else {
				ImGui::TextDisabled("Bloom effect not found.");
			}
		}

	} // namespace UI
} // namespace Boidsish
