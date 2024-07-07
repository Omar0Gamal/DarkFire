#pragma once
#include "Level.h"
#include <chrono>
#include <Core/util/StackTrace.h>

namespace Core
{
	struct Entry
	{
		Level m_level;
		std::wstring m_message;
		const wchar_t* m_sourceFile;
		const wchar_t* m_sourceFunctionName;
		int m_sourceLine;
		std::chrono::system_clock::time_point m_timeStamp;
		std::optional<StackTrace> m_stackTrace;
	};
}