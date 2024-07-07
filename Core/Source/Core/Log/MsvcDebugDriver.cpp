#include "MsvcDebugDriver.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "TextFormatter.h"

namespace Core
{
	MsvcDebugDriver::MsvcDebugDriver(std::shared_ptr<ITextFormatter> formatter)
		:m_formatter{ std::move(formatter) }
	{
	}

	void MsvcDebugDriver::Submit(const Entry& entry)
	{
		if (m_formatter)
		{
			OutputDebugStringW(m_formatter->Format(entry).c_str());
		}
	}

	void MsvcDebugDriver::SetFormatter(std::shared_ptr<ITextFormatter> formatter)
	{
		m_formatter = std::move(formatter);
	}
}
