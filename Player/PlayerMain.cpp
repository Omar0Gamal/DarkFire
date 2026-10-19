#include "Engine/Runtime/EngineContext.h"
#include "Engine/Runtime/Module.h"
#include "Engine/Platform/DynamicLibrary.h"
#include "Engine/Platform/CrashHandler.h"
#include <iostream>
#include <string>
#include <filesystem>

int main(int argc, char** argv)
{
	Platform::CrashHandler::Install();

	auto& ctx = Core::EngineContext::Get();
	ctx.log.info("Player", "DarkFire Player Runtime booting up (v2.0)...");

	std::string projectPath = "Templates/Playground3D/Project.dfproj";
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "--project" && i + 1 < argc)
		{
			projectPath = argv[++i];
		}
	}

	ctx.log.info("Player", "Target Project: {}", projectPath);

	// Determine module path
	std::string configName = "Debug";
#if defined(DF_RELEASE)
	configName = "Release";
#elif defined(DF_DIST)
	configName = "Dist";
#endif

	std::filesystem::path modulePath = "bin/" + configName + "-windows-x86_64/Templates/Playground3D/Playground3D.dll";
	if (!std::filesystem::exists(modulePath))
	{
		modulePath = "Templates/Playground3D/Binaries/" + configName + "-windows-x86_64/Playground3D.dll";
	}
	if (!std::filesystem::exists(modulePath))
	{
		// Try relative to current executable or local directory
		modulePath = "Playground3D.dll";
	}

	ctx.log.info("Player", "Loading game module from: {}", modulePath.string());
	auto libRes = Platform::DynamicLibrary::Load(modulePath);
	if (libRes.isErr())
	{
		ctx.log.error("Player", "Module load error: {}", libRes.error());
		return 1;
	}

	auto& lib = libRes.value();

	auto pfnGetInfo = lib.getFunction<PFN_DF_GetModuleInfo>("DF_GetModuleInfo");
	auto pfnRegisterTypes = lib.getFunction<PFN_DF_RegisterTypes>("DF_RegisterTypes");
	auto pfnOnLoad = lib.getFunction<PFN_DF_OnLoad>("DF_OnLoad");
	auto pfnOnUnload = lib.getFunction<PFN_DF_OnUnload>("DF_OnUnload");

	if (!pfnGetInfo || !pfnOnLoad)
	{
		ctx.log.error("Player", "Module does not export required DarkFire C API endpoints!");
		return 1;
	}

	Core::ModuleInfo info = pfnGetInfo();
	ctx.log.info("Player", "Module Detected: '{}' (v{}) with API Version: {}", info.name, info.version, info.apiVersion);

	if (info.apiVersion != Core::DARKFIRE_API_VERSION)
	{
		ctx.log.error("Player", "API version mismatch! Expected {}, got {}", Core::DARKFIRE_API_VERSION, info.apiVersion);
		return 1;
	}

	if (pfnRegisterTypes)
	{
		pfnRegisterTypes(ctx);
	}

	pfnOnLoad(ctx);

	ctx.log.info("Player", "Module execution verified. Shutting down...");

	if (pfnOnUnload)
	{
		pfnOnUnload();
	}

	lib.unload();
	ctx.log.info("Player", "DarkFire Player exited cleanly with code 0.");
	return 0;
}
