#pragma once

#include <map>
#include <string>

namespace Boidsish {
	class Config {
	public:
		Config(const std::string& filename);

		void Load();
		void Save();

		std::string GetString(const std::string& key, const std::string& default_value) const;
		int         GetInt(const std::string& key, int default_value) const;
		float       GetFloat(const std::string& key, float default_value) const;
		bool        GetBool(const std::string& key, bool default_value) const;

		void SetString(const std::string& key, const std::string& value);
		void SetInt(const std::string& key, int value);
		void SetFloat(const std::string& key, float value);
		void SetBool(const std::string& key, bool value);

	private:
		std::string                        m_filename;
		std::map<std::string, std::string> m_data;
	};
} // namespace Boidsish
