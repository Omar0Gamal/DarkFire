#include "Level.h"

namespace Core
{
	std::wstring GetLevelName(Level level)
	{
		switch (level)
		{
		case Level::Verbose:
			return L"Trace";
		case Level::Debug:
			return L"Debug";
		case Level::Info:
			return L"Info";
		case Level::Warn:
			return L"Warn";
		case Level::Error:
			return L"Error";
		case Level::Fatal:
			return L"Fatal";
		case Level::None:
			return L"";
		default:
			return L"Unknown";
		}
	}
}