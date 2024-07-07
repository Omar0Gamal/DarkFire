#include "TextFormatter.h"
#include "Entry.h"
#include <format>
#include <sstream>

namespace Core
{
	std::wstring DefaultTextFormatter::Format(const Entry& entry) const
	{
		std::wostringstream oss;
		oss << std::format(L"{} [#{}] {}\n  >> at {}\n     {}({})\n",
			std::chrono::zoned_time(std::chrono::current_zone(), entry.m_timeStamp),
			GetLevelName(entry.m_level),
			entry.m_message,
			entry.m_sourceFunctionName,
			entry.m_sourceFile,
			entry.m_sourceLine
		);
		if (entry.m_stackTrace)
		{
			oss << entry.m_stackTrace->print() << '\n';
		}
		return oss.str();
	}
}