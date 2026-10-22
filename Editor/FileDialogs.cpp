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
#include <shobjidl.h>
#include <shlobj.h>
#include <knownfolders.h>

namespace Editor
{
	static std::wstring Utf8FilterToWide(const char* filter)
	{
		if (!filter) return L"";
		// Scan to end of double-null terminated filter string
		size_t len = 0;
		while (filter[len] != '\0' || filter[len + 1] != '\0')
		{
			len++;
		}
		len += 2; // Include both terminating null characters

		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, filter, static_cast<int>(len), nullptr, 0);
		std::wstring result(sizeNeeded, 0);
		MultiByteToWideChar(CP_UTF8, 0, filter, static_cast<int>(len), result.data(), sizeNeeded);
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

		std::wstring wideFilter = Utf8FilterToWide(filter);
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

		std::wstring wideFilter = Utf8FilterToWide(filter);
		ofn.lpstrFilter = wideFilter.c_str();
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		if (GetSaveFileNameW(&ofn) == TRUE)
		{
			return WideToUtf8(ofn.lpstrFile);
		}

		return "";
	}

	std::string FileDialogs::SelectFolder()
	{
		std::string resultPath;
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		const bool needUninit = SUCCEEDED(hr);

		IFileOpenDialog* pFileOpen = nullptr;
		hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
		if (SUCCEEDED(hr))
		{
			DWORD dwOptions = 0;
			if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
			{
				pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
			}

			HWND hwndOwner = Runtime::Application::Get().getWindow().getNativeHandle();
			hr = pFileOpen->Show(hwndOwner);
			if (SUCCEEDED(hr))
			{
				IShellItem* pItem = nullptr;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath = nullptr;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
					if (SUCCEEDED(hr))
					{
						resultPath = WideToUtf8(pszFilePath);
						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}

		if (needUninit)
		{
			CoUninitialize();
		}

		return resultPath;
	}

	std::filesystem::path FileDialogs::GetDocumentsPath()
	{
		PWSTR pPath = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &pPath)))
		{
			std::filesystem::path p(pPath);
			CoTaskMemFree(pPath);
			return p;
		}

		const char* userProfile = std::getenv("USERPROFILE");
		if (userProfile)
		{
			std::filesystem::path p = std::filesystem::path(userProfile) / "Documents";
			if (std::filesystem::exists(p)) return p;
		}

		return "Projects";
	}

	std::filesystem::path FileDialogs::GetDefaultProjectsDirectory()
	{
		std::filesystem::path dir = GetDocumentsPath() / "DarkFireProjects";
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		return dir;
	}
}
