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
			crashLog << "Exception Code: 0x" << std::hex << pExceptionPointers->ExceptionRecord->ExceptionCode << "\n";
			crashLog << "Exception Address: 0x" << pExceptionPointers->ExceptionRecord->ExceptionAddress << std::dec << "\n\n";

			crashLog << "--- Callstack ---\n";
			SymInitialize(GetCurrentProcess(), nullptr, TRUE);
			void* backtrace[32];
			unsigned short frames = CaptureStackBackTrace(0, 32, backtrace, nullptr);
			char symbolBuffer[sizeof(SYMBOL_INFO) + 256];
			SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
			symbol->MaxNameLen = 255;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

			for (unsigned int i = 0; i < frames; ++i)
			{
				DWORD64 displacement = 0;
				if (SymFromAddr(GetCurrentProcess(), (DWORD64)backtrace[i], &displacement, symbol))
				{
					crashLog << "[" << i << "] " << symbol->Name << " + 0x" << std::hex << displacement << std::dec << "\n";
				}
				else
				{
					crashLog << "[" << i << "] 0x" << std::hex << (DWORD64)backtrace[i] << std::dec << "\n";
				}
			}
			crashLog << "\n";

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
