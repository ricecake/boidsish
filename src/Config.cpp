#include "Config.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Boidsish {
	Config::Config(const std::string& filename): m_filename(filename) {}

	void Config::Load() {
		std::ifstream file(m_filename);
		if (!file.is_open()) {
			return;
		}

		std::string line;
		std::string current_section;
		while (std::getline(file, line)) {
			// Trim whitespace
			line.erase(0, line.find_first_not_of(" \t\n\r"));
			line.erase(line.find_last_not_of(" \t\n\r") + 1);

			if (line.empty() || line[0] == '#') {
				continue; // Skip comments and empty lines
			}

			if (line[0] == '[' && line.back() == ']') {
				current_section = line.substr(1, line.length() - 2);
			} else {
				std::stringstream ss(line);
				std::string       key;
				std::string       value;
				if (std::getline(ss, key, '=') && std::getline(ss, value)) {
					// Trim whitespace from key and value
					key.erase(0, key.find_first_not_of(" \t"));
					key.erase(key.find_last_not_of(" \t") + 1);
					value.erase(0, value.find_first_not_of(" \t"));
					value.erase(value.find_last_not_of(" \t") + 1);

					if (!current_section.empty()) {
						m_data[current_section][key] = value;
					}
				}
			}
		}
	}

	void Config::Save() {
		std::ofstream file(m_filename);
		if (!file.is_open()) {
			return;
		}

		for (const auto& section_pair : m_data) {
			file << "[" << section_pair.first << "]" << std::endl;
			for (const auto& key_value_pair : section_pair.second) {
				file << key_value_pair.first << "=" << key_value_pair.second << std::endl;
			}
			file << std::endl; // Add a blank line for readability
		}
	}

	std::string
	Config::GetString(const std::string& section, const std::string& key, const std::string& default_value) {
		auto section_it = m_data.find(section);
		if (section_it != m_data.end()) {
			auto key_it = section_it->second.find(key);
			if (key_it != section_it->second.end()) {
				return key_it->second;
			}
		}
		return default_value;
	}

	int Config::GetInt(const std::string& section, const std::string& key, int default_value) {
		auto section_it = m_data.find(section);
		if (section_it != m_data.end()) {
			auto key_it = section_it->second.find(key);
			if (key_it != section_it->second.end()) {
				return std::stoi(key_it->second);
			}
		}
		return default_value;
	}

	float Config::GetFloat(const std::string& section, const std::string& key, float default_value) {
		auto section_it = m_data.find(section);
		if (section_it != m_data.end()) {
			auto key_it = section_it->second.find(key);
			if (key_it != section_it->second.end()) {
				return std::stof(key_it->second);
			}
		}
		return default_value;
	}

	bool Config::GetBool(const std::string& section, const std::string& key, bool default_value) {
		auto section_it = m_data.find(section);
		if (section_it != m_data.end()) {
			auto key_it = section_it->second.find(key);
			if (key_it != section_it->second.end()) {
				std::string val = key_it->second;
				std::transform(val.begin(), val.end(), val.begin(), ::tolower);
				return val == "true" || val == "1";
			}
		}
		return default_value;
	}

	void Config::SetString(const std::string& section, const std::string& key, const std::string& value) {
		m_data[section][key] = value;
	}

	void Config::SetInt(const std::string& section, const std::string& key, int value) {
		m_data[section][key] = std::to_string(value);
	}

	void Config::SetFloat(const std::string& section, const std::string& key, float value) {
		m_data[section][key] = std::to_string(value);
	}

	void Config::SetBool(const std::string& section, const std::string& key, bool value) {
		m_data[section][key] = value ? "true" : "false";
	}

	std::vector<std::string> Config::GetSections() const {
		std::vector<std::string> sections;
		for (const auto& pair : m_data) {
			sections.push_back(pair.first);
		}
		return sections;
	}

	std::map<std::string, std::string> Config::GetSection(const std::string& section) const {
		auto it = m_data.find(section);
		if (it != m_data.end()) {
			return it->second;
		}
		return {};
	}
} // namespace Boidsish
