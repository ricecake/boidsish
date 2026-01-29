#pragma once

#include <map>
#include <string>
#include <vector>

namespace Boidsish {
	class Config {
	public:
		Config(const std::string& filename);

		void Load();
		void Save();

		std::string
			  GetString(const std::string& section, const std::string& key, const std::string& default_value) const;
		int   GetInt(const std::string& section, const std::string& key, int default_value) const;
		float GetFloat(const std::string& section, const std::string& key, float default_value) const;
		bool  GetBool(const std::string& section, const std::string& key, bool default_value) const;

		void SetString(const std::string& section, const std::string& key, const std::string& value);
		void SetInt(const std::string& section, const std::string& key, int value);
		void SetFloat(const std::string& section, const std::string& key, float value);
		void SetBool(const std::string& section, const std::string& key, bool value);

		std::vector<std::string>           GetSections() const;
		std::map<std::string, std::string> GetSection(const std::string& section) const;

	private:
		std::string                                               m_filename;
		std::map<std::string, std::map<std::string, std::string>> m_data;
	};
} // namespace Boidsish
