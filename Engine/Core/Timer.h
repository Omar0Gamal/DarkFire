#pragma once

#include "Engine/Core/Defines.h"
#include <windows.h>

namespace Core
{
	class DF_API Timer
	{
	public:
		Timer();
		void reset();
		double getElapsedSeconds() const;
		double getElapsedMilliseconds() const;

	private:
		LARGE_INTEGER m_frequency{};
		LARGE_INTEGER m_startTime{};
	};
}
