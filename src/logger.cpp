#include "logger.h"
#include "Config.h"
#include <algorithm>

namespace logger {

	static LogLevel stringToLogLevel(const std::string& str) {
		std::string s = str;
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
		if (s == "info")
			return LogLevel::INFO;
		if (s == "warning")
			return LogLevel::WARNING;
		if (s == "error")
			return LogLevel::ERROR;
		if (s == "debug")
			return LogLevel::DEBUG;
		return LogLevel::LOG;
	}

	void Configure(const Boidsish::Config& cfg) {
		auto section = cfg.GetSection("logging");
		if (section.empty())
			return;

		auto& multi = defaultLogger.backend;
		multi.clearBackends();

		// Console logging
		bool console_enabled = cfg.GetBool("logging", "console_enabled", true);
		if (console_enabled) {
			auto console = std::make_unique<ConsoleBackend>();
			for (int i = 0; i <= (int)LogLevel::DEBUG; ++i) {
				LogLevel    level = (LogLevel)i;
				std::string key   = std::string(levelString(level));
				std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
				key += "_console";
				bool default_val = true;
				// If global "level" is specified, use it as default for all
				std::string global_level_str = cfg.GetString("logging", "console_level", "");
				if (!global_level_str.empty()) {
					LogLevel global_level = stringToLogLevel(global_level_str);
					default_val           = (int)level <= (int)global_level;
				}
				console->setLogLevel(level, cfg.GetBool("logging", key, default_val));
			}
			multi.addBackend(std::move(console));
		}

		// File logging
		std::string log_file = cfg.GetString("logging", "log_file", "");
		if (!log_file.empty()) {
			auto file = std::make_unique<FileBackend>(log_file);
			for (int i = 0; i <= (int)LogLevel::DEBUG; ++i) {
				LogLevel    level = (LogLevel)i;
				std::string key   = std::string(levelString(level));
				std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
				key += "_file";
				file->setLogLevel(level, cfg.GetBool("logging", key, true));
			}
			multi.addBackend(std::move(file));
		}
	}

} // namespace logger
