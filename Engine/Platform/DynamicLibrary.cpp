#include "Engine/Platform/DynamicLibrary.h"

namespace Platform
{
	DynamicLibrary::~DynamicLibrary()
	{
		unload();
	}

	DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
		: m_handle(other.m_handle)
	{
		other.m_handle = nullptr;
	}

	DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept
	{
		if (this != &other)
		{
			unload();
			m_handle = other.m_handle;
			other.m_handle = nullptr;
		}
		return *this;
	}

	Core::Result<DynamicLibrary> DynamicLibrary::Load(const std::filesystem::path& path)
	{
		std::wstring wpath = path.wstring();
		HMODULE handle = LoadLibraryW(wpath.c_str());
		if (!handle)
		{
			DWORD err = GetLastError();
			return Core::Result<DynamicLibrary>::Err("Failed to load dynamic library: " + path.string() + " (Error code: " + std::to_string(err) + ")");
		}
		return Core::Result<DynamicLibrary>::Ok(DynamicLibrary(handle));
	}

	void DynamicLibrary::unload()
	{
		if (m_handle)
		{
			FreeLibrary(m_handle);
			m_handle = nullptr;
		}
	}
}
