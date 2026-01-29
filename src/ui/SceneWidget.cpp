#include "ui/SceneWidget.h"
#include "imgui.h"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Boidsish {
	namespace UI {

		SceneWidget::SceneWidget(SceneManager& sceneManager, Visualizer& visualizer)
			: _sceneManager(sceneManager), _visualizer(visualizer) {
			m_show = true;
			m_saveName[0] = '\0';
			m_saveCamera = true;
			m_moveCamera = true;
			m_newDictName[0] = '\0';
		}

		void SceneWidget::Draw() {
			if (!m_show) return;

			ImGui::Begin("Scene Manager", &m_show);

			// Dictionary Selection
			auto dictionaries = _sceneManager.GetDictionaries();
			std::string currentDict = _sceneManager.GetCurrentDictionaryName();

			if (ImGui::BeginCombo("Dictionary", currentDict.empty() ? "Select..." : currentDict.c_str())) {
				for (const auto& dict : dictionaries) {
					bool isSelected = (currentDict == dict);
					if (ImGui::Selectable(dict.c_str(), isSelected)) {
						_sceneManager.LoadDictionary(dict);
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
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
				if (ImGui::Button("Save")) {
					Scene scene;
					scene.name = m_saveName;
					scene.timestamp = (long long)std::time(nullptr);
					scene.lights = _visualizer.GetLightManager().GetLights();
					scene.ambient_light = _visualizer.GetLightManager().GetAmbientLight();
					if (m_saveCamera) {
						scene.camera = _visualizer.GetCamera();
					}
					_sceneManager.AddScene(scene);
					_sceneManager.SaveDictionary(currentDict);
					m_saveName[0] = '\0';
				}

				ImGui::Separator();

				// Load Scene
				ImGui::Text("Load Scene");
				auto& scenes = _sceneManager.GetScenes();
				static int selectedScene = -1;

				std::string preview = "Select Scene...";
				if (selectedScene >= 0 && selectedScene < (int)scenes.size()) {
					if (!scenes[selectedScene].name.empty()) {
						preview = scenes[selectedScene].name;
					} else {
						std::time_t t = (std::time_t)scenes[selectedScene].timestamp;
						std::tm* tm = std::localtime(&t);
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
							std::time_t t = (std::time_t)scenes[i].timestamp;
							std::tm* tm = std::localtime(&t);
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
				if (ImGui::Button("Load") && selectedScene >= 0) {
					const auto& scene = scenes[selectedScene];
					_visualizer.GetLightManager().GetLights() = scene.lights;
					_visualizer.GetLightManager().SetAmbientLight(scene.ambient_light);
					if (m_moveCamera && scene.camera) {
						_visualizer.SetCamera(*scene.camera);
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

	}
}
