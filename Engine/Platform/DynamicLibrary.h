#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include <windows.h>
#include <string>
#include <filesystem>

namespace Platform
{
	class DF_API DynamicLibrary
	{
	public:
		DynamicLibrary() = default;
		explicit DynamicLibrary(HMODULE handle) : m_handle(handle) {}
		~DynamicLibrary();

		DynamicLibrary(const DynamicLibrary&) = delete;
		DynamicLibrary& operator=(const DynamicLibrary&) = delete;

		DynamicLibrary(DynamicLibrary&& other) noexcept;
		DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

		static Core::Result<DynamicLibrary> Load(const std::filesystem::path& path);
		void unload();

		bool isLoaded() const { return m_handle != nullptr; }

		template<typename Fn>
		Fn getFunction(const char* name) const
		{
			if (!m_handle) return nullptr;
			return reinterpret_cast<Fn>(GetProcAddress(m_handle, name));
		}

	private:
		HMODULE m_handle = nullptr;
	};
}
