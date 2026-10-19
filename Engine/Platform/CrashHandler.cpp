#include "Engine/Platform/CrashHandler.h"
#include "Engine/Core/Log/Log.h"
#include <crtdbg.h>
#include <dbghelp.h>
#include <fstream>
#include <iostream>

#pragma comment(lib, "dbghelp.lib")

namespace Platform
{
	void CrashHandler::Install()
	{
#if defined(DF_DEBUG)
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
		SetUnhandledExceptionFilter(ExceptionFilter);
	}

	LONG WINAPI CrashHandler::ExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers)
	{
		// 1. Write MiniDump
		HANDLE hDumpFile = CreateFileA(
			"DarkFire_Crash.dmp",
			GENERIC_WRITE,
			0,
			nullptr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
			dumpInfo.ThreadId = GetCurrentThreadId();
			dumpInfo.ExceptionPointers = pExceptionPointers;
			dumpInfo.ClientPointers = FALSE;

			MiniDumpWriteDump(
				GetCurrentProcess(),
				GetCurrentProcessId(),
				hDumpFile,
				MiniDumpNormal,
				&dumpInfo,
				nullptr,
				nullptr
			);
			CloseHandle(hDumpFile);
		}

		// 2. Dump last 100 log lines to crash.log
		std::ofstream crashLog("crash.log", std::ios::out);
		if (crashLog.is_open())
		{
			crashLog << "=== DarkFire Engine Crash Report ===\n";
			crashLog << "Exception Code: 0x" << std::hex << pExceptionPointers->ExceptionRecord->ExceptionCode << std::dec << "\n\n";
			crashLog << "--- Last 100 Log Lines ---\n";

			auto messages = Core::Log::Get().getRecentMessages(100);
			for (const auto& msg : messages)
			{
				crashLog << "[" << msg.timestamp << "] [" << msg.category << "] " << msg.text << "\n";
			}
			crashLog.close();
		}

		std::cerr << "\n[CRITICAL] DarkFire Engine has crashed! MiniDump saved to DarkFire_Crash.dmp, crash log written to crash.log.\n";

		return EXCEPTION_CONTINUE_SEARCH;
	}
}
