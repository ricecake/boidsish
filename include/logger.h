#pragma once

#include <concepts>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#if __has_include(<source_location>)
	#include <source_location>
#endif
#include <sstream>
#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
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
		const std::string      message;   // Now potentially interpolated
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
	public:
		virtual ~Backend() = default;
		virtual bool render(const LogLevel level, const std::string_view& str) = 0;
		virtual bool isEnabled(LogLevel level) const = 0;
		virtual void setLogLevel(LogLevel level, bool enabled) = 0;
	};

	class BaseBackend: public Backend {
	protected:
		uint8_t enabled_mask = 0xFF;

	public:
		bool isEnabled(LogLevel level) const override {
			return (enabled_mask & (1 << static_cast<uint8_t>(level))) != 0;
		}

		void setLogLevel(LogLevel level, bool enabled) override {
			if (enabled)
				enabled_mask |= (1 << static_cast<uint8_t>(level));
			else
				enabled_mask &= ~(1 << static_cast<uint8_t>(level));
		}
	};

	class ConsoleBackend: public BaseBackend {
	public:
		bool render(const LogLevel level, const std::string_view& str) override {
			if (!isEnabled(level))
				return false;
			std::cout << str << std::endl;
			return true;
		}
	};

	class FileBackend: public BaseBackend {
		std::ofstream file;

	public:
		FileBackend(const std::string& filename) { file.open(filename, std::ios::app); }

		bool render(const LogLevel level, const std::string_view& str) override {
			if (!isEnabled(level))
				return false;
			if (file.is_open()) {
				file << str << std::endl;
				return true;
			}
			return false;
		}
	};

	class MultiBackend: public Backend {
		std::vector<std::unique_ptr<Backend>> backends;

	public:
		MultiBackend() { backends.push_back(std::make_unique<ConsoleBackend>()); }

		void addBackend(std::unique_ptr<Backend> backend) { backends.push_back(std::move(backend)); }

		void clearBackends() { backends.clear(); }

		bool render(const LogLevel level, const std::string_view& str) override {
			bool success = false;
			for (auto& b : backends) {
				if (b->render(level, str))
					success = true;
			}
			return success;
		}

		bool isEnabled(LogLevel level) const override {
			for (auto& b : backends) {
				if (b->isEnabled(level))
					return true;
			}
			return false;
		}

		void setLogLevel(LogLevel level, bool enabled) override {
			for (auto& b : backends) {
				b->setLogLevel(level, enabled);
			}
		}

		auto& getBackends() { return backends; }
	};

	template <typename T>
	struct is_tuple_like: std::false_type {};

	template <typename... Ts>
	struct is_tuple_like<std::tuple<Ts...>>: std::true_type {};

	template <typename T1, typename T2>
	struct is_tuple_like<std::pair<T1, T2>>: std::true_type {};

	template <typename T, std::size_t N>
	struct is_tuple_like<std::array<T, N>>: std::true_type {};

	template <typename T>
	inline constexpr bool is_tuple_like_v = is_tuple_like<T>::value;

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
	public:
		B backend;

	private:
		template <typename... Ts>
		void doLogging(const LogLevel& level, const LogSource& src, Ts&&... flags) {
			std::string       message(src.msg);
			std::stringstream tags;

			size_t searchPos = 0;
			auto   process = [&](auto&& arg) {
				std::stringstream ss;
				std::ostream&     os = ss;
				using T = std::remove_cvref_t<decltype(arg)>;

				if constexpr (is_tuple_like_v<T>) {
					if constexpr (std::tuple_size_v<T> == 2) {
						os << std::get<0>(arg) << " => [" << std::get<1>(arg) << "]";
					} else {
						os << "{ tuple-like size=" << std::tuple_size_v<T> << " }";
					}
				} else if constexpr (requires { os << arg; }) {
					os << std::forward<decltype(arg)>(arg);
				} else {
					os << "{ unprintable type }";
				}

				std::string replacement = ss.str();
				size_t      pos = message.find("{}", searchPos);
				if (pos != std::string::npos) {
					message.replace(pos, 2, replacement);
					searchPos = pos + replacement.length();
				} else {
					tags << "[" << replacement << "] ";
				}
			};

			(process(std::forward<Ts>(flags)), ...);

			LogMessage log{
				.level = level,
				.message = message,
				.file_name = src.loc.file_name(),
				// .function_name = src.loc.function_name(),
				.tags = tags.str(),
				.line_number = src.loc.line(),
			};

			std::string logStr = format(log);
			backend.render(level, logStr);
		}

	public:
		template <typename... Ts>
		void LOG(LogSource src, Ts&&... flags) {
			if (backend.isEnabled(LogLevel::LOG))
				doLogging(LogLevel::LOG, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void INFO(LogSource src, Ts&&... flags) {
			if (backend.isEnabled(LogLevel::INFO))
				doLogging(LogLevel::INFO, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void WARNING(LogSource src, Ts&&... flags) {
			if (backend.isEnabled(LogLevel::WARNING))
				doLogging(LogLevel::WARNING, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void ERROR(LogSource src, Ts&&... flags) {
			if (backend.isEnabled(LogLevel::ERROR))
				doLogging(LogLevel::ERROR, src, std::forward<Ts>(flags)...);
		};

		template <typename... Ts>
		void DEBUG(LogSource src, Ts&&... flags) {
			if (backend.isEnabled(LogLevel::DEBUG))
				doLogging(LogLevel::DEBUG, src, std::forward<Ts>(flags)...);
		};
	};

	inline Logger<MultiBackend> defaultLogger;

	template <typename... Ts>
	inline void LOG(LogSource src, Ts&&... flags) {
		defaultLogger.LOG(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	inline void ERROR(LogSource src, Ts&&... flags) {
		defaultLogger.ERROR(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	inline void DEBUG(LogSource src, Ts&&... flags) {
		defaultLogger.DEBUG(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	inline void INFO(LogSource src, Ts&&... flags) {
		defaultLogger.INFO(src, std::forward<Ts>(flags)...);
	};

	template <typename... Ts>
	inline void WARNING(LogSource src, Ts&&... flags) {
		defaultLogger.WARNING(src, std::forward<Ts>(flags)...);
	};

}; // namespace logger

namespace Boidsish {
	class Config;
}

namespace logger {
	void Configure(Boidsish::Config& cfg);
}

namespace logger {
	// What if these were classes, whose initializers did the logging?  A lot more would be definitively known at
	// compile time...

}; // namespace logger