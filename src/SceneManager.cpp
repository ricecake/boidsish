#include "SceneManager.h"
#include <filesystem>
#include <chrono>
#include <iostream>

namespace Boidsish {

	SceneManager::SceneManager(const std::string& sceneFolder) : m_sceneFolder(sceneFolder) {
		if (!std::filesystem::exists(m_sceneFolder)) {
			std::filesystem::create_directories(m_sceneFolder);
		}
	}

	std::vector<std::string> SceneManager::GetDictionaries() {
		std::vector<std::string> dictionaries;
		if (!std::filesystem::exists(m_sceneFolder)) return dictionaries;

		for (const auto& entry : std::filesystem::directory_iterator(m_sceneFolder)) {
			if (entry.path().extension() == ".dict") {
				dictionaries.push_back(entry.path().stem().string());
			}
		}
		return dictionaries;
	}

	void SceneManager::LoadDictionary(const std::string& name) {
		m_currentDictionary = name;
		m_scenes.clear();
		std::string path = m_sceneFolder + "/" + name + ".dict";
		if (!std::filesystem::exists(path)) return;

		Config config(path);
		config.Load();

		int num_scenes = config.GetInt("Metadata", "num_scenes", 0);
		for (int i = 0; i < num_scenes; ++i) {
			m_scenes.push_back(DeserializeScene(config, i));
		}
	}

	void SceneManager::SaveDictionary(const std::string& name) {
		if (name.empty()) return;
		m_currentDictionary = name;
		std::string path = m_sceneFolder + "/" + name + ".dict";
		Config config(path);

		config.SetInt("Metadata", "num_scenes", (int)m_scenes.size());
		for (int i = 0; i < m_scenes.size(); ++i) {
			SerializeScene(config, m_scenes[i], i);
		}
		config.Save();
	}

	const std::vector<Scene>& SceneManager::GetScenes() const {
		return m_scenes;
	}

	void SceneManager::AddScene(const Scene& scene) {
		m_scenes.push_back(scene);
	}

	void SceneManager::RemoveScene(int index) {
		if (index >= 0 && index < (int)m_scenes.size()) {
			m_scenes.erase(m_scenes.begin() + index);
		}
	}

	void SceneManager::SerializeScene(Config& config, const Scene& scene, int index) {
		std::string section = "Scene_" + std::to_string(index);
		config.SetString(section, "name", scene.name);
		config.SetString(section, "timestamp", std::to_string(scene.timestamp));
		config.SetInt(section, "num_lights", (int)scene.lights.size());
		config.SetBool(section, "has_camera", scene.camera.has_value());
		config.SetFloat(section, "ambient_r", scene.ambient_light.r);
		config.SetFloat(section, "ambient_g", scene.ambient_light.g);
		config.SetFloat(section, "ambient_b", scene.ambient_light.b);

		if (scene.camera) {
			std::string cam_section = section + "_Camera";
			config.SetFloat(cam_section, "x", scene.camera->x);
			config.SetFloat(cam_section, "y", scene.camera->y);
			config.SetFloat(cam_section, "z", scene.camera->z);
			config.SetFloat(cam_section, "pitch", scene.camera->pitch);
			config.SetFloat(cam_section, "yaw", scene.camera->yaw);
			config.SetFloat(cam_section, "roll", scene.camera->roll);
			config.SetFloat(cam_section, "fov", scene.camera->fov);
		}

		for (int i = 0; i < (int)scene.lights.size(); ++i) {
			std::string light_section = section + "_Light_" + std::to_string(i);
			const auto& l = scene.lights[i];
			config.SetInt(light_section, "type", l.type);
			config.SetFloat(light_section, "pos_x", l.position.x);
			config.SetFloat(light_section, "pos_y", l.position.y);
			config.SetFloat(light_section, "pos_z", l.position.z);
			config.SetFloat(light_section, "color_r", l.color.r);
			config.SetFloat(light_section, "color_g", l.color.g);
			config.SetFloat(light_section, "color_b", l.color.b);
			config.SetFloat(light_section, "intensity", l.base_intensity);
			config.SetFloat(light_section, "dir_x", l.direction.x);
			config.SetFloat(light_section, "dir_y", l.direction.y);
			config.SetFloat(light_section, "dir_z", l.direction.z);
			config.SetFloat(light_section, "inner", l.inner_cutoff);
			config.SetFloat(light_section, "outer", l.outer_cutoff);
			config.SetBool(light_section, "shadows", l.casts_shadow);

			// Behavior
			config.SetInt(light_section, "beh_type", (int)l.behavior.type);
			config.SetFloat(light_section, "beh_period", l.behavior.period);
			config.SetFloat(light_section, "beh_amplitude", l.behavior.amplitude);
			config.SetFloat(light_section, "beh_duty", l.behavior.duty_cycle);
			config.SetFloat(light_section, "beh_flicker", l.behavior.flicker_intensity);
			config.SetString(light_section, "beh_msg", l.behavior.message);
			config.SetBool(light_section, "beh_loop", l.behavior.loop);
		}
	}

	Scene SceneManager::DeserializeScene(const Config& config, int index) {
		Scene scene;
		std::string section = "Scene_" + std::to_string(index);
		scene.name = config.GetString(section, "name", "");
		std::string ts_str = config.GetString(section, "timestamp", "0");
		try {
			scene.timestamp = std::stoll(ts_str);
		} catch (...) {
			scene.timestamp = 0;
		}

		int num_lights = config.GetInt(section, "num_lights", 0);
		bool has_camera = config.GetBool(section, "has_camera", false);
		scene.ambient_light.r = config.GetFloat(section, "ambient_r", 0.1f);
		scene.ambient_light.g = config.GetFloat(section, "ambient_g", 0.1f);
		scene.ambient_light.b = config.GetFloat(section, "ambient_b", 0.1f);

		if (has_camera) {
			std::string cam_section = section + "_Camera";
			Camera cam;
			cam.x = config.GetFloat(cam_section, "x", 0.0f);
			cam.y = config.GetFloat(cam_section, "y", 0.0f);
			cam.z = config.GetFloat(cam_section, "z", 5.0f);
			cam.pitch = config.GetFloat(cam_section, "pitch", 0.0f);
			cam.yaw = config.GetFloat(cam_section, "yaw", 0.0f);
			cam.roll = config.GetFloat(cam_section, "roll", 0.0f);
			cam.fov = config.GetFloat(cam_section, "fov", 45.0f);
			scene.camera = cam;
		}

		for (int i = 0; i < num_lights; ++i) {
			std::string light_section = section + "_Light_" + std::to_string(i);
			Light l;
			l.type = config.GetInt(light_section, "type", POINT_LIGHT);
			l.position.x = config.GetFloat(light_section, "pos_x", 0.0f);
			l.position.y = config.GetFloat(light_section, "pos_y", 0.0f);
			l.position.z = config.GetFloat(light_section, "pos_z", 0.0f);
			l.color.r = config.GetFloat(light_section, "color_r", 1.0f);
			l.color.g = config.GetFloat(light_section, "color_g", 1.0f);
			l.color.b = config.GetFloat(light_section, "color_b", 1.0f);
			l.base_intensity = config.GetFloat(light_section, "intensity", 1.0f);
			l.intensity = l.base_intensity;
			l.direction.x = config.GetFloat(light_section, "dir_x", 0.0f);
			l.direction.y = config.GetFloat(light_section, "dir_y", -1.0f);
			l.direction.z = config.GetFloat(light_section, "dir_z", 0.0f);
			l.inner_cutoff = config.GetFloat(light_section, "inner", 0.9f);
			l.outer_cutoff = config.GetFloat(light_section, "outer", 0.8f);
			l.casts_shadow = config.GetBool(light_section, "shadows", false);

			// Behavior
			l.behavior.type = (LightBehaviorType)config.GetInt(light_section, "beh_type", 0);
			l.behavior.period = config.GetFloat(light_section, "beh_period", 1.0f);
			l.behavior.amplitude = config.GetFloat(light_section, "beh_amplitude", 1.0f);
			l.behavior.duty_cycle = config.GetFloat(light_section, "beh_duty", 0.5f);
			l.behavior.flicker_intensity = config.GetFloat(light_section, "beh_flicker", 0.0f);
			l.behavior.message = config.GetString(light_section, "beh_msg", "");
			l.behavior.loop = config.GetBool(light_section, "beh_loop", true);
			l.behavior.timer = 0.0f;
			l.behavior.morse_index = -1;

			scene.lights.push_back(l);
		}

		return scene;
	}

}
