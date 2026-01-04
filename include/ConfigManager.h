#pragma once

#include "Config.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace Boidsish {

    struct ConfigValue {
        enum class Type { STRING, INT, FLOAT, BOOL };
        Type type;
        std::string string_value;
        int int_value;
        float float_value;
        bool bool_value;
    };

    class ConfigManager {
    public:
        static ConfigManager& GetInstance();

        void Initialize(const std::string& app_name);
        void Shutdown();

        // Getters with section override logic
        std::string GetAppSettingString(const std::string& key, const std::string& default_value);
        int         GetAppSettingInt(const std::string& key, int default_value);
        float       GetAppSettingFloat(const std::string& key, float default_value);
        bool        GetAppSettingBool(const std::string& key, bool default_value);

        // Global-only getters
        std::string GetGlobalSettingString(const std::string& key, const std::string& default_value);
        int         GetGlobalSettingInt(const std::string& key, int default_value);
        float       GetGlobalSettingFloat(const std::string& key, float default_value);
        bool        GetGlobalSettingBool(const std::string& key, bool default_value);


        // Setters - always write to the app-specific section
        void SetString(const std::string& key, const std::string& value);
        void SetInt(const std::string& key, int value);
        void SetFloat(const std::string& key, float value);
        void SetBool(const std::string& key, bool value);

        // For UI
        std::vector<std::string> GetSections() const;
        std::map<std::string, std::string> GetSectionContents(const std::string& section) const;
        std::map<std::string, ConfigValue> GetRegisteredValues(const std::string& section);


    private:
        ConfigManager();
        ~ConfigManager();
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        void RegisterKey(const std::string& section, const std::string& key, ConfigValue::Type type, const std::string& default_value);
        void RegisterKey(const std::string& section, const std::string& key, ConfigValue::Type type, int default_value);
        void RegisterKey(const std::string& section, const std::string& key, ConfigValue::Type type, float default_value);
        void RegisterKey(const std::string& section, const std::string& key, ConfigValue::Type type, bool default_value);


        static ConfigManager* m_instance;
        static std::mutex m_mutex;

        Config m_config;
        std::string m_appName;
        std::string m_appSection;

        // Track all keys accessed to show them in the UI
        std::map<std::string, ConfigValue> m_registeredGlobalKeys;
        std::map<std::string, ConfigValue> m_registeredAppKeys;
    };

} // namespace Boidsish
