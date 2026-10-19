#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <format>
#include <cstdint>

namespace Core
{
	enum class LogLevel : uint8_t
	{
		Trace = 0,
		Info,
		Warn,
		Error,
		Fatal,
		None
	};

	struct LogMessage
	{
		LogLevel level;
		std::string category;
		std::string text;
		std::string timestamp;
	};

	class DF_API Log
	{
	public:
		static Log& Get();

		void setLogLevel(LogLevel level) { m_minLevel = level; }
		LogLevel getLogLevel() const { return m_minLevel; }
		bool isLevelEnabled(LogLevel level) const { return level >= m_minLevel; }

		template<typename... Args>
		void trace(std::string_view category, std::format_string<Args...> fmt, Args&&... args)
		{
			if (isLevelEnabled(LogLevel::Trace))
			{
				logInternal(LogLevel::Trace, category, std::format(fmt, std::forward<Args>(args)...));
			}
		}

		template<typename... Args>
		void info(std::string_view category, std::format_string<Args...> fmt, Args&&... args)
		{
			if (isLevelEnabled(LogLevel::Info))
			{
				logInternal(LogLevel::Info, category, std::format(fmt, std::forward<Args>(args)...));
			}
		}

		template<typename... Args>
		void warn(std::string_view category, std::format_string<Args...> fmt, Args&&... args)
		{
			if (isLevelEnabled(LogLevel::Warn))
			{
				logInternal(LogLevel::Warn, category, std::format(fmt, std::forward<Args>(args)...));
			}
		}

		template<typename... Args>
		void error(std::string_view category, std::format_string<Args...> fmt, Args&&... args)
		{
			if (isLevelEnabled(LogLevel::Error))
			{
				logInternal(LogLevel::Error, category, std::format(fmt, std::forward<Args>(args)...));
			}
		}

		template<typename... Args>
		void fatal(std::string_view category, std::format_string<Args...> fmt, Args&&... args)
		{
			if (isLevelEnabled(LogLevel::Fatal))
			{
				logInternal(LogLevel::Fatal, category, std::format(fmt, std::forward<Args>(args)...));
			}
		}

		// Retrieve last N messages for crash dumps or Editor console
		std::vector<LogMessage> getRecentMessages(size_t maxCount = 100);

	private:
		Log();
		~Log();

		Log(const Log&) = delete;
		Log& operator=(const Log&) = delete;

		void logInternal(LogLevel level, std::string_view category, std::string message);

		LogLevel m_minLevel = LogLevel::Trace;
		std::mutex m_mutex;
		std::vector<LogMessage> m_ringBuffer;
		size_t m_ringHead = 0;
		bool m_ringFull = false;
		static constexpr size_t RING_CAPACITY = 1000;
	};
}

// Lazy evaluation log macros
#define DF_LOG_TRACE(category, fmt, ...) \
	do { \
		if (::Core::Log::Get().isLevelEnabled(::Core::LogLevel::Trace)) \
			::Core::Log::Get().trace(category, fmt, ##__VA_ARGS__); \
	} while (0)

#define DF_LOG_INFO(category, fmt, ...) \
	do { \
		if (::Core::Log::Get().isLevelEnabled(::Core::LogLevel::Info)) \
			::Core::Log::Get().info(category, fmt, ##__VA_ARGS__); \
	} while (0)

#define DF_LOG_WARN(category, fmt, ...) \
	do { \
		if (::Core::Log::Get().isLevelEnabled(::Core::LogLevel::Warn)) \
			::Core::Log::Get().warn(category, fmt, ##__VA_ARGS__); \
	} while (0)

#define DF_LOG_ERROR(category, fmt, ...) \
	do { \
		if (::Core::Log::Get().isLevelEnabled(::Core::LogLevel::Error)) \
			::Core::Log::Get().error(category, fmt, ##__VA_ARGS__); \
	} while (0)

#define DF_LOG_FATAL(category, fmt, ...) \
	do { \
		if (::Core::Log::Get().isLevelEnabled(::Core::LogLevel::Fatal)) \
			::Core::Log::Get().fatal(category, fmt, ##__VA_ARGS__); \
	} while (0)
