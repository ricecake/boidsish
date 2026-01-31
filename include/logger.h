#pragma once

#include <cstdint>
#include <iostream>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>         // Add this
using namespace std::literals; // required for ""sv

namespace logger {
	enum class LogLevel : uint8_t { LOG, INFO, WARNING, ERR, DEBUG };

	constexpr std::string_view levelString(const LogLevel& level) {
		switch (level) {
		case LogLevel::LOG:
			return "LOG"sv;
		case LogLevel::ERR:
			return "ERROR"sv;
		case LogLevel::DEBUG:
			return "DEBUG"sv;
		}
		return "LOG"sv;
	}

	struct LogMessage {
		const LogLevel         level = LogLevel::LOG;
		const std::string_view message;   // View of the original message
		const std::string_view file_name; // View of the const char*
		// const std::string_view function_name; // View of the const char*
		const std::string  tags;
		const unsigned int line_number;
	};

	inline const std::string format(const LogMessage& msg) {
		std::stringstream str;
		str << "[" << levelString(msg.level) << "] " << msg.message;
		if (!msg.tags.empty()) {
			str << " " << msg.tags;
		}
		str << " (" << msg.file_name << ":" << msg.line_number << ")";

		// if (msg.file_name.length() && bool(msg.line_number) && msg.function_name.length()) {
		// 	str << " (" << msg.file_name << ":" << msg.line_number << " :: " << msg.function_name << ")";
		// }
		return str.str();
	}

	class Backend { // abstract base class for backend
	protected:
		virtual ~Backend() = default;
		virtual bool render(const std::string_view& str) = 0;
	};

	class ConsoleBackend: public Backend {
	public:
		bool render(const std::string_view& str) override {
			std::cout << str << std::endl;
			return true;
		}
	};

	struct LogSource {
		std::string_view     msg;
		std::source_location loc;

		template <typename StringType>
		constexpr LogSource(const StringType& m, const std::source_location& l = std::source_location::current()):
			msg(m), loc(l) {}
	};

	template <class B>
		requires std::derived_from<B, Backend>
	class Logger {
		B backend;

		template <typename... Ts>
		void doLogging(const LogLevel& level, const LogSource& src, Ts&&... flags) {
			std::stringstream tags;
			((tags << "[" << flags << "] "), ...);
			LogMessage log{
				.level = level,
				.message = src.msg,
				.file_name = src.loc.file_name(),
				// .function_name = src.loc.function_name(),
				.tags = tags.str(),
				.line_number = src.loc.line(),
			};

			std::string logStr = format(log);
			backend.render(logStr);
		}

	public:
		template <typename... Ts>
		void LOG(LogSource& src, Ts&&... flags) {
			doLogging(LogLevel::LOG, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void INFO(LogSource& src, Ts&&... flags) {
			doLogging(LogLevel::INFO, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void WARNING(LogSource& src, Ts&&... flags) {
			doLogging(LogLevel::WARNING, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void ERR(LogSource& src, Ts&&... flags) {
			doLogging(LogLevel::ERR, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void DEBUG(LogSource& src, Ts&&... flags) {
			doLogging(LogLevel::DEBUG, src, std::forward<Ts>(flags)...);
		};
	};

	inline static Logger<ConsoleBackend> defaultLogger;

	template <typename... Ts>
	void LOG(LogSource src, Ts&&... flags) {
		defaultLogger.LOG(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	void ERR(LogSource src, Ts&&... flags) {
		defaultLogger.ERR(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	void DEBUG(LogSource src, Ts&&... flags) {
		defaultLogger.DEBUG(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	void INFO(LogSource src, Ts&&... flags) {
		defaultLogger.INFO(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	void WARNING(LogSource src, Ts&&... flags) {
		defaultLogger.WARNING(src, std::forward<Ts>(flags)...);
	};

	// What if these were classes, whose initializers did the logging?  A lot more would be definitively known at
	// compile time...

}; // namespace logger