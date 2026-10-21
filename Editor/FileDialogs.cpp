#include "Editor/FileDialogs.h"
#include "Engine/Runtime/Application.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

namespace Editor
{
	static std::wstring Utf8ToWide(std::string_view utf8Str)
	{
		if (utf8Str.empty()) return L"";
		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), nullptr, 0);
		std::wstring result(sizeNeeded, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), result.data(), sizeNeeded);
		return result;
	}

	static std::string WideToUtf8(std::wstring_view wideStr)
	{
		if (wideStr.empty()) return "";
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), static_cast<int>(wideStr.size()), nullptr, 0, nullptr, nullptr);
		std::string result(sizeNeeded, 0);
		WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), static_cast<int>(wideStr.size()), result.data(), sizeNeeded, nullptr, nullptr);
		return result;
	}

	std::string FileDialogs::OpenFile(const char* filter)
	{
		OPENFILENAMEW ofn{};
		wchar_t szFile[MAX_PATH] = { 0 };

		ofn.lStructSize = sizeof(OPENFILENAMEW);
		ofn.hwndOwner = Runtime::Application::Get().getWindow().getNativeHandle();
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);

		std::wstring wideFilter = Utf8ToWide(filter);
		ofn.lpstrFilter = wideFilter.c_str();
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameW(&ofn) == TRUE)
		{
			return WideToUtf8(ofn.lpstrFile);
		}

		return "";
	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		OPENFILENAMEW ofn{};
		wchar_t szFile[MAX_PATH] = { 0 };

		ofn.lStructSize = sizeof(OPENFILENAMEW);
		ofn.hwndOwner = Runtime::Application::Get().getWindow().getNativeHandle();
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);

		std::wstring wideFilter = Utf8ToWide(filter);
		ofn.lpstrFilter = wideFilter.c_str();
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		if (GetSaveFileNameW(&ofn) == TRUE)
		{
			return WideToUtf8(ofn.lpstrFile);
		}

		return "";
	}
}
