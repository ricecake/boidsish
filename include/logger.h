#pragma once

#include <cstdint>
#include <iostream>
#if __has_include(<source_location>)
	#include <source_location>
#endif
#include <sstream>
#include <string>
#include <string_view>         // Add this
using namespace std::literals; // required for ""sv

namespace logger {
#if defined(__cpp_lib_source_location)
	using source_location_type = std::source_location;
#else
	struct source_location_type {
		static constexpr source_location_type current() noexcept { return {}; }

		constexpr const char* file_name() const noexcept { return "unknown"; }

		constexpr uint32_t line() const noexcept { return 0; }
	};
#endif

	enum class LogLevel : uint8_t { LOG, INFO, WARNING, ERROR, DEBUG };

	constexpr std::string_view levelString(const LogLevel& level) {
		switch (level) {
		case LogLevel::LOG:
			return "LOG"sv;
		case LogLevel::INFO:
			return "INFO"sv;
		case LogLevel::WARNING:
			return "WARNING"sv;
		case LogLevel::ERROR:
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
		source_location_type loc;

		template <typename StringType>
		constexpr LogSource(const StringType& m, const source_location_type& l = source_location_type::current()):
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
		void ERROR(LogSource& src, Ts&&... flags) {
			doLogging(LogLevel::ERROR, src, std::forward<Ts>(flags)...);
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
	void ERROR(LogSource src, Ts&&... flags) {
		defaultLogger.ERROR(src, std::forward<Ts>(flags)...);
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