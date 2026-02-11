#include "SceneManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace Boidsish {

	SceneManager::SceneManager(const std::string& sceneFolder): m_sceneFolder(sceneFolder) {
		if (!std::filesystem::exists(m_sceneFolder)) {
			std::filesystem::create_directories(m_sceneFolder);
		}
	}

	std::vector<std::string> SceneManager::GetDictionaries() {
		std::vector<std::string> dictionaries;
		if (!std::filesystem::exists(m_sceneFolder))
			return dictionaries;

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
		if (!std::filesystem::exists(path))
			return;

		Config config(path);
		config.Load();

		int num_scenes = config.GetInt("Metadata", "num_scenes", 0);
		for (int i = 0; i < num_scenes; ++i) {
			m_scenes.push_back(DeserializeScene(config, i));
		}
	}

	void SceneManager::SaveDictionary(const std::string& name) {
		if (name.empty())
			return;
		m_currentDictionary = name;
		std::string path = m_sceneFolder + "/" + name + ".dict";
		Config      config(path);

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

		// Object Effects
		std::string obj_section = section + "_ObjectEffects";
		config.SetBool(obj_section, "ripple", scene.object_effects.ripple);
		config.SetBool(obj_section, "color_shift", scene.object_effects.color_shift);
		config.SetBool(obj_section, "black_and_white", scene.object_effects.black_and_white);
		config.SetBool(obj_section, "negative", scene.object_effects.negative);
		config.SetBool(obj_section, "shimmery", scene.object_effects.shimmery);
		config.SetBool(obj_section, "glitched", scene.object_effects.glitched);
		config.SetBool(obj_section, "wireframe", scene.object_effects.wireframe);

		// Post Processing
		std::string pp_section = section + "_PostProcessing";
		config.SetBool(pp_section, "bloom_enabled", scene.post_processing.bloom_enabled);
		config.SetFloat(pp_section, "bloom_intensity", scene.post_processing.bloom_intensity);
		config.SetFloat(pp_section, "bloom_threshold", scene.post_processing.bloom_threshold);
		config.SetBool(pp_section, "atmosphere_enabled", scene.post_processing.atmosphere_enabled);
		config.SetFloat(pp_section, "atmosphere_density", scene.post_processing.atmosphere_density);
		config.SetFloat(pp_section, "fog_density", scene.post_processing.fog_density);
		config.SetFloat(pp_section, "mie_anisotropy", scene.post_processing.mie_anisotropy);
		config.SetFloat(pp_section, "sun_intensity_factor", scene.post_processing.sun_intensity_factor);
		config.SetFloat(pp_section, "cloud_density", scene.post_processing.cloud_density);
		config.SetFloat(pp_section, "cloud_altitude", scene.post_processing.cloud_altitude);
		config.SetFloat(pp_section, "cloud_thickness", scene.post_processing.cloud_thickness);
		config.SetFloat(pp_section, "cloud_r", scene.post_processing.cloud_color.r);
		config.SetFloat(pp_section, "cloud_g", scene.post_processing.cloud_color.g);
		config.SetFloat(pp_section, "cloud_b", scene.post_processing.cloud_color.b);
		config.SetBool(pp_section, "tone_mapping_enabled", scene.post_processing.tone_mapping_enabled);
		config.SetBool(pp_section, "film_grain_enabled", scene.post_processing.film_grain_enabled);
		config.SetFloat(pp_section, "film_grain_intensity", scene.post_processing.film_grain_intensity);
		config.SetBool(pp_section, "ssao_enabled", scene.post_processing.ssao_enabled);
		config.SetFloat(pp_section, "ssao_radius", scene.post_processing.ssao_radius);
		config.SetFloat(pp_section, "ssao_bias", scene.post_processing.ssao_bias);
		config.SetFloat(pp_section, "ssao_intensity", scene.post_processing.ssao_intensity);
		config.SetFloat(pp_section, "ssao_power", scene.post_processing.ssao_power);
		config.SetBool(pp_section, "negative_enabled", scene.post_processing.negative_enabled);
		config.SetBool(pp_section, "glitch_enabled", scene.post_processing.glitch_enabled);
		config.SetBool(pp_section, "optical_flow_enabled", scene.post_processing.optical_flow_enabled);
		config.SetBool(pp_section, "strobe_enabled", scene.post_processing.strobe_enabled);
		config.SetBool(pp_section, "whisp_trail_enabled", scene.post_processing.whisp_trail_enabled);
		config.SetBool(pp_section, "time_stutter_enabled", scene.post_processing.time_stutter_enabled);

		if (scene.camera) {
			std::string cam_section = section + "_Camera";
			config.SetFloat(cam_section, "x", scene.camera->x);
			config.SetFloat(cam_section, "y", scene.camera->y);
			config.SetFloat(cam_section, "z", scene.camera->z);
			config.SetFloat(cam_section, "pitch", scene.camera->pitch);
			config.SetFloat(cam_section, "yaw", scene.camera->yaw);
			config.SetFloat(cam_section, "roll", scene.camera->roll);
			config.SetFloat(cam_section, "fov", scene.camera->fov);
			config.SetFloat(cam_section, "follow_distance", scene.camera->follow_distance);
			config.SetFloat(cam_section, "follow_elevation", scene.camera->follow_elevation);
			config.SetFloat(cam_section, "follow_look_ahead", scene.camera->follow_look_ahead);
			config.SetFloat(cam_section, "follow_responsiveness", scene.camera->follow_responsiveness);
			config.SetFloat(cam_section, "path_smoothing", scene.camera->path_smoothing);
			config.SetFloat(cam_section, "path_bank_factor", scene.camera->path_bank_factor);
			config.SetFloat(cam_section, "path_bank_speed", scene.camera->path_bank_speed);
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
		Scene       scene;
		std::string section = "Scene_" + std::to_string(index);
		scene.name = config.GetString(section, "name", "");
		std::string ts_str = config.GetString(section, "timestamp", "0");
		try {
			scene.timestamp = std::stoll(ts_str);
		} catch (...) {
			scene.timestamp = 0;
		}

		int  num_lights = config.GetInt(section, "num_lights", 0);
		bool has_camera = config.GetBool(section, "has_camera", false);
		scene.ambient_light.r = config.GetFloat(section, "ambient_r", 0.1f);
		scene.ambient_light.g = config.GetFloat(section, "ambient_g", 0.1f);
		scene.ambient_light.b = config.GetFloat(section, "ambient_b", 0.1f);

		// Object Effects
		std::string obj_section = section + "_ObjectEffects";
		scene.object_effects.ripple = config.GetBool(obj_section, "ripple", false);
		scene.object_effects.color_shift = config.GetBool(obj_section, "color_shift", false);
		scene.object_effects.black_and_white = config.GetBool(obj_section, "black_and_white", false);
		scene.object_effects.negative = config.GetBool(obj_section, "negative", false);
		scene.object_effects.shimmery = config.GetBool(obj_section, "shimmery", false);
		scene.object_effects.glitched = config.GetBool(obj_section, "glitched", false);
		scene.object_effects.wireframe = config.GetBool(obj_section, "wireframe", false);

		// Post Processing
		std::string pp_section = section + "_PostProcessing";
		scene.post_processing.bloom_enabled = config.GetBool(pp_section, "bloom_enabled", false);
		scene.post_processing.bloom_intensity = config.GetFloat(pp_section, "bloom_intensity", 0.1f);
		scene.post_processing.bloom_threshold = config.GetFloat(pp_section, "bloom_threshold", 1.0f);
		scene.post_processing.atmosphere_enabled = config.GetBool(pp_section, "atmosphere_enabled", true);
		scene.post_processing.atmosphere_density = config.GetFloat(pp_section, "atmosphere_density", 1.0f);
		scene.post_processing.fog_density = config.GetFloat(pp_section, "fog_density", 1.0f);
		scene.post_processing.mie_anisotropy = config.GetFloat(pp_section, "mie_anisotropy", 0.80f);
		scene.post_processing.sun_intensity_factor = config.GetFloat(pp_section, "sun_intensity_factor", 15.0f);
		scene.post_processing.cloud_density = config.GetFloat(pp_section, "cloud_density", 0.2f);
		scene.post_processing.cloud_altitude = config.GetFloat(pp_section, "cloud_altitude", 2.0f);
		scene.post_processing.cloud_thickness = config.GetFloat(pp_section, "cloud_thickness", 0.5f);
		scene.post_processing.cloud_color.r = config.GetFloat(pp_section, "cloud_r", 0.95f);
		scene.post_processing.cloud_color.g = config.GetFloat(pp_section, "cloud_g", 0.95f);
		scene.post_processing.cloud_color.b = config.GetFloat(pp_section, "cloud_b", 1.0f);
		scene.post_processing.tone_mapping_enabled = config.GetBool(pp_section, "tone_mapping_enabled", false);
		scene.post_processing.film_grain_enabled = config.GetBool(pp_section, "film_grain_enabled", false);
		scene.post_processing.film_grain_intensity = config.GetFloat(pp_section, "film_grain_intensity", 0.02f);
		scene.post_processing.ssao_enabled = config.GetBool(pp_section, "ssao_enabled", false);
		scene.post_processing.ssao_radius = config.GetFloat(pp_section, "ssao_radius", 0.5f);
		scene.post_processing.ssao_bias = config.GetFloat(pp_section, "ssao_bias", 0.1f);
		scene.post_processing.ssao_intensity = config.GetFloat(pp_section, "ssao_intensity", 1.0f);
		scene.post_processing.ssao_power = config.GetFloat(pp_section, "ssao_power", 1.0f);
		scene.post_processing.negative_enabled = config.GetBool(pp_section, "negative_enabled", false);
		scene.post_processing.glitch_enabled = config.GetBool(pp_section, "glitch_enabled", false);
		scene.post_processing.optical_flow_enabled = config.GetBool(pp_section, "optical_flow_enabled", false);
		scene.post_processing.strobe_enabled = config.GetBool(pp_section, "strobe_enabled", false);
		scene.post_processing.whisp_trail_enabled = config.GetBool(pp_section, "whisp_trail_enabled", false);
		scene.post_processing.time_stutter_enabled = config.GetBool(pp_section, "time_stutter_enabled", false);

		if (has_camera) {
			std::string cam_section = section + "_Camera";
			Camera      cam;
			cam.x = config.GetFloat(cam_section, "x", 0.0f);
			cam.y = config.GetFloat(cam_section, "y", 0.0f);
			cam.z = config.GetFloat(cam_section, "z", 5.0f);
			cam.pitch = config.GetFloat(cam_section, "pitch", 0.0f);
			cam.yaw = config.GetFloat(cam_section, "yaw", 0.0f);
			cam.roll = config.GetFloat(cam_section, "roll", 0.0f);
			cam.fov = config.GetFloat(cam_section, "fov", 45.0f);
			cam.follow_distance = config.GetFloat(
				cam_section,
				"follow_distance",
				Constants::Project::Camera::ChaseTrailBehind()
			);
			cam.follow_elevation = config.GetFloat(
				cam_section,
				"follow_elevation",
				Constants::Project::Camera::ChaseElevation()
			);
			cam.follow_look_ahead = config.GetFloat(
				cam_section,
				"follow_look_ahead",
				Constants::Project::Camera::ChaseLookAhead()
			);
			cam.follow_responsiveness = config.GetFloat(
				cam_section,
				"follow_responsiveness",
				Constants::Project::Camera::ChaseResponsiveness()
			);
			cam.path_smoothing = config.GetFloat(
				cam_section,
				"path_smoothing",
				Constants::Project::Camera::PathFollowSmoothing()
			);
			cam.path_bank_factor = config.GetFloat(
				cam_section,
				"path_bank_factor",
				Constants::Project::Camera::PathBankFactor()
			);
			cam.path_bank_speed = config.GetFloat(
				cam_section,
				"path_bank_speed",
				Constants::Project::Camera::PathBankSpeed()
			);
			scene.camera = cam;
		}

		for (int i = 0; i < num_lights; ++i) {
			std::string light_section = section + "_Light_" + std::to_string(i);
			Light       l;
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

} // namespace Boidsish
