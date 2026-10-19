#include "Engine/Runtime/EngineContext.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Runtime/Config.h"
#include "Engine/Runtime/GameModuleLoader.h"
#include "Engine/Runtime/SceneRuntimeLayer.h"
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
	int maxFrames = -1;

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "--project" && i + 1 < argc)
		{
			projectPath = argv[++i];
		}
		else if (arg == "--frames" && i + 1 < argc)
		{
			maxFrames = std::stoi(argv[++i]);
		}
	}

	ctx.log.info("Player", "Target Project: {}", projectPath);

	std::filesystem::path projFilePath(projectPath);
	if (!std::filesystem::exists(projFilePath))
	{
		ctx.log.error("Player", "Project file not found: {}", projectPath);
		return 1;
	}

	auto configRes = Runtime::ProjectConfig::Load(projFilePath);
	if (configRes.isErr())
	{
		ctx.log.error("Player", "Failed to load project config: {}", configRes.error());
		return 1;
	}

	auto config = configRes.value();
	std::filesystem::path projectDir = projFilePath.parent_path();

	// Resolve config name
	std::string configName = "Debug";
#if defined(DF_ASAN)
	configName = "Debug-ASan";
#elif defined(DF_RELEASE)
	configName = "Release";
#elif defined(DF_DIST)
	configName = "Dist";
#endif

	// Resolve module path
	std::string modPathStr = config.modulePath;
	size_t tokenPos = modPathStr.find("%CONFIG%");
	if (tokenPos != std::string::npos)
	{
		modPathStr.replace(tokenPos, 8, configName);
	}

	std::filesystem::path modulePath = modPathStr;
	if (!std::filesystem::exists(modulePath))
	{
		// Try relative to project directory
		modulePath = projectDir / modPathStr;
	}
	if (!std::filesystem::exists(modulePath))
	{
		// Try root bin directory
		modulePath = "bin/" + configName + "-windows-x86_64/Templates/" + config.name + "/" + config.name + ".dll";
	}

	ctx.log.info("Player", "Loading game module from: {}", modulePath.string());
	auto moduleRes = Runtime::GameModuleLoader::Load(modulePath, ctx);
	if (moduleRes.isErr())
	{
		ctx.log.error("Player", "Failed to load game module: {}", moduleRes.error());
		return 1;
	}

	auto gameModule = std::move(moduleRes.value());
	ctx.log.info("Player", "Loaded Module: '{}' (v{})", gameModule.getInfo().name, gameModule.getInfo().version);

	// Create and initialize Application
	Runtime::Application app(config);
	if (!app.init())
	{
		ctx.log.fatal("Player", "Application initialization failed!");
		return 1;
	}

	// Create Scene Runtime Layer
	auto sceneLayer = std::make_shared<Runtime::SceneRuntimeLayer>();
	std::filesystem::path scenePath = projectDir / config.startScene;
	if (std::filesystem::exists(scenePath))
	{
		ctx.log.info("Player", "Loading start scene: {}", scenePath.string());
		sceneLayer->loadScene(scenePath);
	}
	else
	{
		ctx.log.warn("Player", "Start scene '{}' not found. Running with empty scene.", scenePath.string());
	}

	app.pushLayer(sceneLayer);

	if (maxFrames > 0)
	{
		ctx.log.info("Player", "Running in bounded frame test mode (maxFrames = {})...", maxFrames);
		for (int frame = 0; frame < maxFrames; ++frame)
		{
			app.getWindow().pollEvents();
			sceneLayer->onFixedUpdate(1.0f / 60.0f);
			sceneLayer->onUpdate(1.0f / 60.0f);
		}
		ctx.log.info("Player", "Bounded frame execution completed successfully.");
	}
	else
	{
		app.run();
	}

	ctx.log.info("Player", "Player shutting down cleanly.");
	return 0;
}
