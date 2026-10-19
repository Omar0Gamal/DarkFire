#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include "Engine/Platform/DynamicLibrary.h"
#include "Engine/Runtime/Module.h"
#include <filesystem>
#include <string>

namespace Runtime
{
	class DF_API GameModuleLoader
	{
	public:
		GameModuleLoader() = default;
		~GameModuleLoader();

		GameModuleLoader(const GameModuleLoader&) = delete;
		GameModuleLoader& operator=(const GameModuleLoader&) = delete;

		GameModuleLoader(GameModuleLoader&& other) noexcept;
		GameModuleLoader& operator=(GameModuleLoader&& other) noexcept;

		static Core::Result<GameModuleLoader, std::string> Load(
			const std::filesystem::path& modulePath,
			Core::EngineContext& ctx
		);

		void unload();

		bool isLoaded() const noexcept { return m_library.isLoaded(); }
		const Core::ModuleInfo& getInfo() const noexcept { return m_info; }
		Platform::DynamicLibrary& getLibrary() noexcept { return m_library; }

	private:
		Platform::DynamicLibrary m_library;
		Core::ModuleInfo m_info{};
		PFN_DF_OnUnload m_pfnOnUnload = nullptr;
	};
}
