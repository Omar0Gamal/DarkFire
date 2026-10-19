#pragma once

#include "Engine/Core/Defines.h"
#include <windows.h>

namespace Platform
{
	class DF_API CrashHandler
	{
	public:
		static void Install();
	private:
		static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers);
	};
}
