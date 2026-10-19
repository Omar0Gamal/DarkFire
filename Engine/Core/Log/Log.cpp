#include "Engine/Core/Log/Log.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <windows.h>

namespace Core
{
	namespace
	{
		const char* LevelToString(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Trace: return "TRACE";
			case LogLevel::Info:  return "INFO";
			case LogLevel::Warn:  return "WARN";
			case LogLevel::Error: return "ERROR";
			case LogLevel::Fatal: return "FATAL";
			default:              return "UNKNOWN";
			}
		}

		std::string GetCurrentTimestamp()
		{
			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
			tm localTm;
			localtime_s(&localTm, &time);
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03lld",
				localTm.tm_hour, localTm.tm_min, localTm.tm_sec, static_cast<long long>(ms.count()));
			return std::string(buf);
		}
	}

	Log::Log()
	{
		m_ringBuffer.resize(RING_CAPACITY);
	}

	Log::~Log() = default;

	Log& Log::Get()
	{
		static Log s_instance;
		return s_instance;
	}

	void Log::logInternal(LogLevel level, std::string_view category, std::string message)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		std::string timeStr = GetCurrentTimestamp();
		std::string catStr(category);

		// Format output line: [HH:MM:SS.mmm] [LEVEL] [Category] message\n
		std::string formatted = std::format("[{}] [{}] [{}] {}\n",
			timeStr, LevelToString(level), catStr, message);

		// 1. Stdout
		std::cout << formatted;

		// 2. OutputDebugStringA for Visual Studio output window
		OutputDebugStringA(formatted.c_str());

		// 3. File sink (append)
		std::ofstream logFile("DarkFire.log", std::ios::app);
		if (logFile.is_open())
		{
			logFile << formatted;
		}

		// 4. Ring buffer sink
		m_ringBuffer[m_ringHead] = LogMessage{
			level,
			std::move(catStr),
			std::move(message),
			std::move(timeStr)
		};
		m_ringHead = (m_ringHead + 1) % RING_CAPACITY;
		if (m_ringHead == 0)
		{
			m_ringFull = true;
		}
	}

	std::vector<LogMessage> Log::getRecentMessages(size_t maxCount)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<LogMessage> result;
		size_t total = m_ringFull ? RING_CAPACITY : m_ringHead;
		size_t count = std::min(maxCount, total);
		result.reserve(count);

		size_t startIndex = (m_ringHead + RING_CAPACITY - count) % RING_CAPACITY;
		for (size_t i = 0; i < count; ++i)
		{
			size_t idx = (startIndex + i) % RING_CAPACITY;
			result.push_back(m_ringBuffer[idx]);
		}
		return result;
	}

	void ReportAssertionFailure(const char* expr, const char* msg, const char* file, int line)
	{
		Log::Get().fatal("Assert", "ASSERTION FAILED: {} | {} ({}:{})", expr, msg, file, line);
	}

	void ReportVerifyFailure(const char* expr, const char* msg, const char* file, int line)
	{
		Log::Get().error("Verify", "VERIFY FAILED: {} | {} ({}:{})", expr, msg, file, line);
	}
}
