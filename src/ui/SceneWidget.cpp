#include "ui/SceneWidget.h"

#include <ctime>
#include <iomanip>
#include <sstream>

#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/SsaoEffect.h"

namespace Boidsish {
	namespace UI {

		SceneWidget::SceneWidget(SceneManager& sceneManager, Visualizer& visualizer):
			_sceneManager(sceneManager), _visualizer(visualizer) {
			m_show = true;
			m_saveName[0] = '\0';
			m_saveCamera = true;
			m_moveCamera = true;
			m_saveEffects = true;
			m_applyEffects = true;
			m_newDictName[0] = '\0';
		}

		void SceneWidget::Draw() {
			if (!m_show)
				return;

			ImGui::Begin("Scene Manager", &m_show);

			// Dictionary Selection
			auto        dictionaries = _sceneManager.GetDictionaries();
			std::string currentDict = _sceneManager.GetCurrentDictionaryName();

			if (ImGui::BeginCombo("Dictionary", currentDict.empty() ? "Select..." : currentDict.c_str())) {
				for (const auto& dict : dictionaries) {
					bool isSelected = (currentDict == dict);
					if (ImGui::Selectable(dict.c_str(), isSelected)) {
						_sceneManager.LoadDictionary(dict);
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::InputText("New Dictionary", m_newDictName, sizeof(m_newDictName));
			ImGui::SameLine();
			if (ImGui::Button("Create")) {
				_sceneManager.SaveDictionary(m_newDictName);
				m_newDictName[0] = '\0';
			}

			ImGui::Separator();

			if (!currentDict.empty()) {
				// Save Scene
				ImGui::Text("Save Current Scene");
				ImGui::InputText("Scene Name", m_saveName, sizeof(m_saveName));
				ImGui::Checkbox("Save Camera", &m_saveCamera);
				ImGui::Checkbox("Save Effects", &m_saveEffects);
				if (ImGui::Button("Save")) {
					Scene scene;
					scene.name = m_saveName;
					scene.timestamp = (long long)std::time(nullptr);
					scene.lights = _visualizer.GetLightManager().GetLights();
					scene.ambient_light = _visualizer.GetLightManager().GetAmbientLight();
					if (m_saveCamera) {
						scene.camera = _visualizer.GetCamera();
					}

					if (m_saveEffects) {
						// Object Effects
						scene.object_effects.ripple = _visualizer.IsRippleEffectEnabled();
						scene.object_effects.color_shift = _visualizer.IsColorShiftEffectEnabled();
						scene.object_effects.black_and_white = _visualizer.IsBlackAndWhiteEffectEnabled();
						scene.object_effects.negative = _visualizer.IsNegativeEffectEnabled();
						scene.object_effects.shimmery = _visualizer.IsShimmeryEffectEnabled();
						scene.object_effects.glitched = _visualizer.IsGlitchedEffectEnabled();
						scene.object_effects.wireframe = _visualizer.IsWireframeEffectEnabled();

						// Post Processing
						auto& ppm = _visualizer.GetPostProcessingManager();
						for (auto& effect : ppm.GetPreToneMappingEffects()) {
							if (auto bloom = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect)) {
								scene.post_processing.bloom_enabled = bloom->IsEnabled();
								scene.post_processing.bloom_intensity = bloom->GetIntensity();
								scene.post_processing.bloom_threshold = bloom->GetThreshold();
							} else if (auto atmos = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(
										   effect
									   )) {
								scene.post_processing.atmosphere_enabled = atmos->IsEnabled();
								scene.post_processing.haze_density = atmos->GetHazeDensity();
								scene.post_processing.haze_height = atmos->GetHazeHeight();
								scene.post_processing.haze_color = atmos->GetHazeColor();
								scene.post_processing.cloud_density = atmos->GetCloudDensity();
								scene.post_processing.cloud_altitude = atmos->GetCloudAltitude();
								scene.post_processing.cloud_thickness = atmos->GetCloudThickness();
								scene.post_processing.cloud_color = atmos->GetCloudColor();
							} else if (auto fg = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect)) {
								scene.post_processing.film_grain_enabled = fg->IsEnabled();
								scene.post_processing.film_grain_intensity = fg->GetIntensity();
							} else if (auto ssao = std::dynamic_pointer_cast<PostProcessing::SsaoEffect>(effect)) {
								scene.post_processing.ssao_enabled = ssao->IsEnabled();
								scene.post_processing.ssao_radius = ssao->GetRadius();
								scene.post_processing.ssao_bias = ssao->GetBias();
								scene.post_processing.ssao_intensity = ssao->GetIntensity();
							} else if (effect->GetName() == "Negative") {
								scene.post_processing.negative_enabled = effect->IsEnabled();
							} else if (effect->GetName() == "Glitch") {
								scene.post_processing.glitch_enabled = effect->IsEnabled();
							} else if (effect->GetName() == "OpticalFlow") {
								scene.post_processing.optical_flow_enabled = effect->IsEnabled();
							} else if (effect->GetName() == "Strobe") {
								scene.post_processing.strobe_enabled = effect->IsEnabled();
							} else if (effect->GetName() == "WhispTrail") {
								scene.post_processing.whisp_trail_enabled = effect->IsEnabled();
							} else if (effect->GetName() == "TimeStutter") {
								scene.post_processing.time_stutter_enabled = effect->IsEnabled();
							}
						}
						if (auto tm = ppm.GetToneMappingEffect()) {
							scene.post_processing.tone_mapping_enabled = tm->IsEnabled();
						}
					}

					_sceneManager.AddScene(scene);
					_sceneManager.SaveDictionary(currentDict);
					m_saveName[0] = '\0';
				}

				ImGui::Separator();

				// Load Scene
				ImGui::Text("Load Scene");
				auto&      scenes = _sceneManager.GetScenes();
				static int selectedScene = -1;

				std::string preview = "Select Scene...";
				if (selectedScene >= 0 && selectedScene < (int)scenes.size()) {
					if (!scenes[selectedScene].name.empty()) {
						preview = scenes[selectedScene].name;
					} else {
						std::time_t       t = (std::time_t)scenes[selectedScene].timestamp;
						std::tm*          tm = std::localtime(&t);
						std::stringstream ss;
						ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
						preview = ss.str();
					}
				}

				if (ImGui::BeginCombo("Scenes", preview.c_str())) {
					for (int i = 0; i < (int)scenes.size(); ++i) {
						std::string label;
						if (!scenes[i].name.empty()) {
							label = scenes[i].name;
						} else {
							std::time_t       t = (std::time_t)scenes[i].timestamp;
							std::tm*          tm = std::localtime(&t);
							std::stringstream ss;
							ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
							label = ss.str();
						}
						bool isSelected = (selectedScene == i);
						if (ImGui::Selectable(label.c_str(), isSelected)) {
							selectedScene = i;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::Checkbox("Move Camera", &m_moveCamera);
				ImGui::Checkbox("Apply Effects", &m_applyEffects);
				if (ImGui::Button("Load") && selectedScene >= 0) {
					const auto& scene = scenes[selectedScene];
					_visualizer.GetLightManager().GetLights() = scene.lights;
					_visualizer.GetLightManager().SetAmbientLight(scene.ambient_light);
					if (m_moveCamera && scene.camera) {
						_visualizer.SetCamera(*scene.camera);
					}

					if (m_applyEffects) {
						// Object Effects
						_visualizer.SetEffectEnabled(VisualEffect::RIPPLE, scene.object_effects.ripple);
						_visualizer.SetEffectEnabled(VisualEffect::COLOR_SHIFT, scene.object_effects.color_shift);
						_visualizer.SetEffectEnabled(
							VisualEffect::BLACK_AND_WHITE,
							scene.object_effects.black_and_white
						);
						_visualizer.SetEffectEnabled(VisualEffect::NEGATIVE, scene.object_effects.negative);
						_visualizer.SetEffectEnabled(VisualEffect::SHIMMERY, scene.object_effects.shimmery);
						_visualizer.SetEffectEnabled(VisualEffect::GLITCHED, scene.object_effects.glitched);
						_visualizer.SetEffectEnabled(VisualEffect::WIREFRAME, scene.object_effects.wireframe);

						// Post Processing
						auto& ppm = _visualizer.GetPostProcessingManager();
						for (auto& effect : ppm.GetPreToneMappingEffects()) {
							if (auto bloom = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect)) {
								bloom->SetEnabled(scene.post_processing.bloom_enabled);
								bloom->SetIntensity(scene.post_processing.bloom_intensity);
								bloom->SetThreshold(scene.post_processing.bloom_threshold);
							} else if (auto atmos = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(
										   effect
									   )) {
								atmos->SetEnabled(scene.post_processing.atmosphere_enabled);
								atmos->SetHazeDensity(scene.post_processing.haze_density);
								atmos->SetHazeHeight(scene.post_processing.haze_height);
								atmos->SetHazeColor(scene.post_processing.haze_color);
								atmos->SetCloudDensity(scene.post_processing.cloud_density);
								atmos->SetCloudAltitude(scene.post_processing.cloud_altitude);
								atmos->SetCloudThickness(scene.post_processing.cloud_thickness);
								atmos->SetCloudColor(scene.post_processing.cloud_color);
							} else if (auto fg = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect)) {
								fg->SetEnabled(scene.post_processing.film_grain_enabled);
								fg->SetIntensity(scene.post_processing.film_grain_intensity);
							} else if (auto ssao = std::dynamic_pointer_cast<PostProcessing::SsaoEffect>(effect)) {
								ssao->SetEnabled(scene.post_processing.ssao_enabled);
								ssao->SetRadius(scene.post_processing.ssao_radius);
								ssao->SetBias(scene.post_processing.ssao_bias);
								ssao->SetIntensity(scene.post_processing.ssao_intensity);
							} else if (effect->GetName() == "Negative") {
								effect->SetEnabled(scene.post_processing.negative_enabled);
							} else if (effect->GetName() == "Glitch") {
								effect->SetEnabled(scene.post_processing.glitch_enabled);
							} else if (effect->GetName() == "OpticalFlow") {
								effect->SetEnabled(scene.post_processing.optical_flow_enabled);
							} else if (effect->GetName() == "Strobe") {
								effect->SetEnabled(scene.post_processing.strobe_enabled);
							} else if (effect->GetName() == "WhispTrail") {
								effect->SetEnabled(scene.post_processing.whisp_trail_enabled);
							} else if (effect->GetName() == "TimeStutter") {
								effect->SetEnabled(scene.post_processing.time_stutter_enabled);
							}
						}
						if (auto tm = ppm.GetToneMappingEffect()) {
							tm->SetEnabled(scene.post_processing.tone_mapping_enabled);
						}
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Remove") && selectedScene >= 0) {
					_sceneManager.RemoveScene(selectedScene);
					_sceneManager.SaveDictionary(currentDict);
					selectedScene = -1;
				}
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
