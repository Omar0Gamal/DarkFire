#pragma once

#if defined(_WIN32) || defined(_WIN64)
	#define DF_PLATFORM_WINDOWS 1
#else
	#error "DarkFire Engine only supports Windows 10/11 x64."
#endif

#if defined(DF_STATIC)
	#define DF_API
	#define DF_MODULE_API
#elif defined(DF_BUILD_DLL)
	#define DF_API __declspec(dllexport)
	#define DF_MODULE_API __declspec(dllimport)
#else
	#define DF_API __declspec(dllimport)
	#define DF_MODULE_API __declspec(dllexport)
#endif

namespace Core
{
	DF_API void ReportAssertionFailure(const char* expr, const char* msg, const char* file, int line);
	DF_API void ReportVerifyFailure(const char* expr, const char* msg, const char* file, int line);
}

// Assertion & Break Macros
#if defined(DF_DEBUG)
	#define DF_DEBUGBREAK() __debugbreak()
	#define DF_ASSERT(expr, msg) \
		do { \
			if (!(expr)) { \
				::Core::ReportAssertionFailure(#expr, msg, __FILE__, __LINE__); \
				DF_DEBUGBREAK(); \
			} \
		} while (0)
	#define DF_VERIFY(expr, msg) DF_ASSERT(expr, msg)
#elif defined(DF_RELEASE)
	#define DF_DEBUGBREAK() ((void)0)
	#define DF_ASSERT(expr, msg) ((void)0)
	#define DF_VERIFY(expr, msg) \
		do { \
			if (!(expr)) { \
				::Core::ReportVerifyFailure(#expr, msg, __FILE__, __LINE__); \
			} \
		} while (0)
#else // DF_DIST
	#define DF_DEBUGBREAK() ((void)0)
	#define DF_ASSERT(expr, msg) ((void)0)
	#define DF_VERIFY(expr, msg) ((void)(expr))
#endif

// Tracy Profiling Wrappers
#if defined(TRACY_ENABLE)
	#include "tracy/Tracy.hpp"
	#define DF_PROFILE_SCOPE(name) ZoneScopedN(name)
	#define DF_PROFILE_FRAME(name) FrameMarkNamed(name)
#else
	#define DF_PROFILE_SCOPE(name) ((void)0)
	#define DF_PROFILE_FRAME(name) ((void)0)
#endif
