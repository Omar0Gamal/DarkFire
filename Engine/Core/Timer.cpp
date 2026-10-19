#include "Engine/Core/Timer.h"

namespace Core
{
	Timer::Timer()
	{
		QueryPerformanceFrequency(&m_frequency);
		reset();
	}

	void Timer::reset()
	{
		QueryPerformanceCounter(&m_startTime);
	}

	double Timer::getElapsedSeconds() const
	{
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		return static_cast<double>(currentTime.QuadPart - m_startTime.QuadPart) / static_cast<double>(m_frequency.QuadPart);
	}

	double Timer::getElapsedMilliseconds() const
	{
		return getElapsedSeconds() * 1000.0;
	}
}
