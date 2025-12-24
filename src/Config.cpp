#include "Config.h"

#include <fstream>
#include <sstream>

namespace Boidsish {
    Config::Config(const std::string& filename) : m_filename(filename) {}

    void Config::Load() {
        std::ifstream file(m_filename);
        if (!file.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string key;
            std::string value;
            if (std::getline(ss, key, '=') && std::getline(ss, value)) {
                m_data[key] = value;
            }
        }
    }

    void Config::Save() {
        std::ofstream file(m_filename);
        if (!file.is_open()) {
            return;
        }

        for (const auto& pair : m_data) {
            file << pair.first << "=" << pair.second << std::endl;
        }
    }

    std::string Config::GetString(const std::string& key, const std::string& default_value) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            return it->second;
        }
        return default_value;
    }

    int Config::GetInt(const std::string& key, int default_value) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            return std::stoi(it->second);
        }
        return default_value;
    }

    float Config::GetFloat(const std::string& key, float default_value) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            return std::stof(it->second);
        }
        return default_value;
    }

    bool Config::GetBool(const std::string& key, bool default_value) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            return it->second == "true";
        }
        return default_value;
    }

    void Config::SetString(const std::string& key, const std::string& value) {
        m_data[key] = value;
    }

    void Config::SetInt(const std::string& key, int value) {
        m_data[key] = std::to_string(value);
    }

    void Config::SetFloat(const std::string& key, float value) {
        m_data[key] = std::to_string(value);
    }

    void Config::SetBool(const std::string& key, bool value) {
        m_data[key] = value ? "true" : "false";
    }
}
