#include "ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace Boidsish {

	ConfigManager* ConfigManager::m_instance = nullptr;
	std::mutex     ConfigManager::m_mutex;

	ConfigManager& ConfigManager::GetInstance() {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_instance == nullptr) {
			m_instance = new ConfigManager();
		}
		return *m_instance;
	}

	ConfigManager::ConfigManager(): m_config("boidsish.ini") {
		m_config.Load();
	}

	ConfigManager::~ConfigManager() {
		m_config.Save();
	}

	void ConfigManager::Initialize(const std::string& app_name) {
		m_appName = app_name;

		// Sanitize app_name to create a valid section name
		m_appSection = app_name;
		std::transform(m_appSection.begin(), m_appSection.end(), m_appSection.begin(), [](unsigned char c) {
			return std::tolower(c);
		});
		std::replace_if(
			m_appSection.begin(),
			m_appSection.end(),
			[](unsigned char c) { return !std::isalnum(c); },
			'_'
		);

		// Remove consecutive underscores
		m_appSection.erase(
			std::unique(m_appSection.begin(), m_appSection.end(), [](char a, char b) { return a == '_' && b == '_'; }),
			m_appSection.end()
		);

		// Pre-register app-specific keys to ensure they appear in the UI
		GetAppSettingBool("enable_terrain", true);
		GetAppSettingBool("enable_floor", true);
		GetAppSettingBool("enable_skybox", true);
		GetAppSettingBool("enable_effects", true);
		GetAppSettingBool("enable_floor_reflection", true);
		GetAppSettingBool("enable_gl_debug", false); // OpenGL debug output (performance impact)
		GetAppSettingBool("render_terrain", true);
		GetAppSettingBool("render_floor", true);
		GetAppSettingBool("render_skybox", true);
		GetAppSettingBool("artistic_effect_ripple", false);
		GetAppSettingBool("artistic_effect_color_shift", false);
		GetAppSettingBool("artistic_effect_black_and_white", false);
		GetAppSettingBool("artistic_effect_negative", false);
		GetAppSettingBool("artistic_effect_shimmery", false);
		GetAppSettingBool("artistic_effect_glitched", false);
		GetAppSettingBool("artistic_effect_wireframe", false);
	}

	void ConfigManager::Shutdown() {
		m_config.Save();
	}

	// --- App-specific Getters ---

	std::string ConfigManager::GetAppSettingString(const std::string& key, const std::string& default_value) {
		RegisterKey(m_appSection, key, ConfigValue::Type::STRING, default_value);
		return m_config.GetString(m_appSection, key, m_config.GetString("global", key, default_value));
	}

	int ConfigManager::GetAppSettingInt(const std::string& key, int default_value) {
		RegisterKey(m_appSection, key, ConfigValue::Type::INT, default_value);
		return m_config.GetInt(m_appSection, key, m_config.GetInt("global", key, default_value));
	}

	float ConfigManager::GetAppSettingFloat(const std::string& key, float default_value) {
		RegisterKey(m_appSection, key, ConfigValue::Type::FLOAT, default_value);
		return m_config.GetFloat(m_appSection, key, m_config.GetFloat("global", key, default_value));
	}

	bool ConfigManager::GetAppSettingBool(const std::string& key, bool default_value) {
		RegisterKey(m_appSection, key, ConfigValue::Type::BOOL, default_value);
		return m_config.GetBool(m_appSection, key, m_config.GetBool("global", key, default_value));
	}

	// --- Global-only Getters ---

	std::string ConfigManager::GetGlobalSettingString(const std::string& key, const std::string& default_value) {
		RegisterKey("global", key, ConfigValue::Type::STRING, default_value);
		return m_config.GetString("global", key, default_value);
	}

	int ConfigManager::GetGlobalSettingInt(const std::string& key, int default_value) {
		RegisterKey("global", key, ConfigValue::Type::INT, default_value);
		return m_config.GetInt("global", key, default_value);
	}

	float ConfigManager::GetGlobalSettingFloat(const std::string& key, float default_value) {
		RegisterKey("global", key, ConfigValue::Type::FLOAT, default_value);
		return m_config.GetFloat("global", key, default_value);
	}

	bool ConfigManager::GetGlobalSettingBool(const std::string& key, bool default_value) {
		RegisterKey("global", key, ConfigValue::Type::BOOL, default_value);
		return m_config.GetBool("global", key, default_value);
	}

	// --- Setters ---

	void ConfigManager::SetString(const std::string& key, const std::string& value) {
		m_config.SetString(m_appSection, key, value);
		m_config.Save();
	}

	void ConfigManager::SetInt(const std::string& key, int value) {
		m_config.SetInt(m_appSection, key, value);
		m_config.Save();
	}

	void ConfigManager::SetFloat(const std::string& key, float value) {
		m_config.SetFloat(m_appSection, key, value);
		m_config.Save();
	}

	void ConfigManager::SetBool(const std::string& key, bool value) {
		m_config.SetBool(m_appSection, key, value);
		m_config.Save();
	}

	// --- UI Helpers ---

	const std::string& ConfigManager::GetAppSectionName() const {
		return m_appSection;
	}

	std::vector<std::string> ConfigManager::GetSections() const {
		return m_config.GetSections();
	}

	std::map<std::string, std::string> ConfigManager::GetSectionContents(const std::string& section) const {
		return m_config.GetSection(section);
	}

	std::map<std::string, ConfigValue> ConfigManager::GetRegisteredValues(const std::string& section) {
		if (section == "global") {
			return m_registeredGlobalKeys;
		} else if (section == m_appSection) {
			return m_registeredAppKeys;
		}
		return {};
	}

	// --- Private Methods ---

	void ConfigManager::RegisterKey(
		const std::string& section,
		const std::string& key,
		ConfigValue::Type  type,
		const std::string& default_value
	) {
		if (m_appSection.empty())
			return;

		auto& map_to_use = (section == "global") ? m_registeredGlobalKeys : m_registeredAppKeys;

		if (map_to_use.find(key) == map_to_use.end()) {
			ConfigValue val;
			val.type = type;
			val.string_value = default_value;
			map_to_use[key] = val;
		}
	}

	void ConfigManager::RegisterKey(
		const std::string& section,
		const std::string& key,
		ConfigValue::Type  type,
		int                default_value
	) {
		if (m_appSection.empty())
			return;

		auto& map_to_use = (section == "global") ? m_registeredGlobalKeys : m_registeredAppKeys;

		if (map_to_use.find(key) == map_to_use.end()) {
			ConfigValue val;
			val.type = type;
			val.int_value = default_value;
			map_to_use[key] = val;
		}
	}

	void ConfigManager::RegisterKey(
		const std::string& section,
		const std::string& key,
		ConfigValue::Type  type,
		float              default_value
	) {
		if (m_appSection.empty())
			return;

		auto& map_to_use = (section == "global") ? m_registeredGlobalKeys : m_registeredAppKeys;

		if (map_to_use.find(key) == map_to_use.end()) {
			ConfigValue val;
			val.type = type;
			val.float_value = default_value;
			map_to_use[key] = val;
		}
	}

	void ConfigManager::RegisterKey(
		const std::string& section,
		const std::string& key,
		ConfigValue::Type  type,
		bool               default_value
	) {
		if (m_appSection.empty())
			return;

		auto& map_to_use = (section == "global") ? m_registeredGlobalKeys : m_registeredAppKeys;

		if (map_to_use.find(key) == map_to_use.end()) {
			ConfigValue val;
			val.type = type;
			val.bool_value = default_value;
			map_to_use[key] = val;
		}
	}

} // namespace Boidsish
