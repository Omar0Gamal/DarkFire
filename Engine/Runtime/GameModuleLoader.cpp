#include "Engine/Runtime/GameModuleLoader.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"

namespace Runtime
{
	GameModuleLoader::~GameModuleLoader()
	{
		unload();
	}

	GameModuleLoader::GameModuleLoader(GameModuleLoader&& other) noexcept
		: m_library(std::move(other.m_library))
		, m_info(other.m_info)
		, m_pfnOnUnload(other.m_pfnOnUnload)
	{
		other.m_info = Core::ModuleInfo{};
		other.m_pfnOnUnload = nullptr;
	}

	GameModuleLoader& GameModuleLoader::operator=(GameModuleLoader&& other) noexcept
	{
		if (this != &other)
		{
			unload();
			m_library = std::move(other.m_library);
			m_info = other.m_info;
			m_pfnOnUnload = other.m_pfnOnUnload;

			other.m_info = Core::ModuleInfo{};
			other.m_pfnOnUnload = nullptr;
		}
		return *this;
	}

	Core::Result<GameModuleLoader, std::string> GameModuleLoader::Load(
		const std::filesystem::path& modulePath,
		Core::EngineContext& ctx
	)
	{
		auto libRes = Platform::DynamicLibrary::Load(modulePath);
		if (libRes.isErr())
		{
			return Core::Result<GameModuleLoader, std::string>::Err(libRes.error());
		}

		auto library = std::move(libRes.value());

		auto pfnGetInfo = library.getFunction<PFN_DF_GetModuleInfo>("DF_GetModuleInfo");
		auto pfnRegisterTypes = library.getFunction<PFN_DF_RegisterTypes>("DF_RegisterTypes");
		auto pfnOnLoad = library.getFunction<PFN_DF_OnLoad>("DF_OnLoad");
		auto pfnOnUnload = library.getFunction<PFN_DF_OnUnload>("DF_OnUnload");

		if (!pfnGetInfo || !pfnOnLoad)
		{
			return Core::Result<GameModuleLoader, std::string>::Err(
				"Game module is missing required entry points (DF_GetModuleInfo, DF_OnLoad)"
			);
		}

		Core::ModuleInfo info = pfnGetInfo();
		if (info.apiVersion != Core::DARKFIRE_API_VERSION)
		{
			return Core::Result<GameModuleLoader, std::string>::Err(
				"Game module API version mismatch! Expected " +
				std::to_string(Core::DARKFIRE_API_VERSION) + ", got " + std::to_string(info.apiVersion)
			);
		}

		if (pfnRegisterTypes)
		{
			pfnRegisterTypes(ctx);
		}

		pfnOnLoad(ctx);

		GameModuleLoader loader;
		loader.m_library = std::move(library);
		loader.m_info = info;
		loader.m_pfnOnUnload = pfnOnUnload;

		return Core::Result<GameModuleLoader, std::string>::Ok(std::move(loader));
	}

	void GameModuleLoader::unload()
	{
		if (m_library.isLoaded())
		{
			if (m_pfnOnUnload)
			{
				m_pfnOnUnload();
				m_pfnOnUnload = nullptr;
			}
			Core::ScriptRegistry::Get().clear();
			m_library.unload();
		}
		m_info = Core::ModuleInfo{};
	}
}
