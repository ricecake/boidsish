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

	void Configure(Boidsish::Config& cfg) {
		bool was_empty = !cfg.HasSection("logging");

		// Establish default values if they don't exist
		if (!cfg.HasKey("logging", "console_enabled"))
			cfg.SetBool("logging", "console_enabled", true);
		if (!cfg.HasKey("logging", "output_file"))
			cfg.SetString("logging", "output_file", "");

		// For each level, set default enabled state for both console and file
		for (int i = 0; i <= (int)LogLevel::DEBUG; ++i) {
			LogLevel    level = (LogLevel)i;
			std::string key   = std::string(levelString(level));
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });

			std::string console_key = key + "_console";
			if (!cfg.HasKey("logging", console_key)) {
				// INFO and below enabled by default for console, DEBUG disabled
				cfg.SetBool("logging", console_key, level != LogLevel::DEBUG);
			}

			std::string file_key = key + "_file";
			if (!cfg.HasKey("logging", file_key)) {
				cfg.SetBool("logging", file_key, true);
			}
		}

		// Save the config if we added any defaults
		// This makes the config file self-documenting for logging
		cfg.Save();

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
				console->setLogLevel(level, cfg.GetBool("logging", key, true));
			}
			multi.addBackend(std::move(console));
		}

		// File logging
		std::string log_file = cfg.GetString("logging", "output_file", "");
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
