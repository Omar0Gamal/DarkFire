#include "Engine/Runtime/EngineContext.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Runtime/Config.h"
#include "Engine/Runtime/GameModuleLoader.h"
#include "Engine/Runtime/SceneRuntimeLayer.h"
#include "Engine/Core/Assets/AssetManager.h"
#include "Engine/Platform/CrashHandler.h"
#include <iostream>
#include <string>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Request high-performance discrete GPU on hybrid / Optimus laptops
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

static std::filesystem::path GetExecutablePath()
{
#ifdef _WIN32
	wchar_t path[MAX_PATH];
	DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (len > 0)
	{
		return std::filesystem::path(path);
	}
#endif
	return std::filesystem::current_path() / "Player.exe";
}

int main(int argc, char** argv)
{
	Platform::CrashHandler::Install();

	auto& ctx = Core::EngineContext::Get();
	ctx.log.info("Player", "DarkFire Player Runtime booting up (v2.0)...");

	try
	{
		std::filesystem::path exePath = GetExecutablePath();
		std::filesystem::path exeDir = exePath.parent_path();
		std::string exeStem = exePath.stem().string();

		// Mount Engine.pak and Game.pak if present
		if (std::filesystem::exists(exeDir / "Engine.pak"))
		{
			ctx.vfs.mountPak("/engine", exeDir / "Engine.pak");
			ctx.log.info("Player", "Mounted Engine.pak from exe directory: {}", (exeDir / "Engine.pak").string());
		}
		else if (std::filesystem::exists("Engine.pak"))
		{
			ctx.vfs.mountPak("/engine", "Engine.pak");
			ctx.log.info("Player", "Mounted Engine.pak from current directory.");
		}

		if (std::filesystem::exists(exeDir / "Game.pak"))
		{
			ctx.vfs.mountPak("", exeDir / "Game.pak");
			ctx.log.info("Player", "Mounted Game.pak from exe directory: {}", (exeDir / "Game.pak").string());
		}
		else if (std::filesystem::exists("Game.pak"))
		{
			ctx.vfs.mountPak("", "Game.pak");
			ctx.log.info("Player", "Mounted Game.pak from current directory.");
		}
		std::string projectPath;
		std::string overrideScene;
		std::string screenshotPath;
		int overrideDebugView = -1;

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
			else if (arg == "--scene" && i + 1 < argc)
			{
				overrideScene = argv[++i];
			}
			else if (arg == "--screenshot" && i + 1 < argc)
			{
				screenshotPath = argv[++i];
			}
			else if (arg == "--debug-view" && i + 1 < argc)
			{
				overrideDebugView = std::stoi(argv[++i]);
			}
		}

		// If no explicit project path, look in exeDir
		if (projectPath.empty())
		{
			if (std::filesystem::exists(exeDir / "Project.dfproj"))
			{
				projectPath = (exeDir / "Project.dfproj").string();
			}
			else if (std::filesystem::exists(exeDir / (exeStem + ".dfproj")))
			{
				projectPath = (exeDir / (exeStem + ".dfproj")).string();
			}
			else if (std::filesystem::exists("Project.dfproj"))
			{
				projectPath = "Project.dfproj";
			}
			else
			{
				std::error_code ec;
				for (const auto& entry : std::filesystem::directory_iterator(exeDir, ec))
				{
					if (entry.is_regular_file() && entry.path().extension() == ".dfproj")
					{
						projectPath = entry.path().string();
						break;
					}
				}
			}
		}

		Runtime::ProjectConfig config{};
		bool configLoaded = false;
		std::filesystem::path projectDir = exeDir;

		// 1. Try loading from physical disk file
		if (!projectPath.empty() && std::filesystem::exists(projectPath))
		{
			ctx.log.info("Player", "Target Project: {}", projectPath);
			auto configRes = Runtime::ProjectConfig::Load(projectPath);
			if (configRes.isOk())
			{
				config = configRes.value();
				configLoaded = true;
				projectDir = std::filesystem::absolute(projectPath).parent_path();
			}
			else
			{
				ctx.log.warn("Player", "Failed to load project config from disk: {}", configRes.error());
			}
		}

		// 2. Try loading from mounted Game.pak VFS
		if (!configLoaded && ctx.vfs.exists("/Project.dfproj"))
		{
			ctx.log.info("Player", "Loading project configuration from Game.pak (/Project.dfproj)...");
			auto fileRes = ctx.vfs.readTextFile("/Project.dfproj");
			if (fileRes.isOk())
			{
				auto configRes = Runtime::ProjectConfig::LoadFromString(fileRes.value());
				if (configRes.isOk())
				{
					config = configRes.value();
					configLoaded = true;
				}
				else
				{
					ctx.log.warn("Player", "Failed to parse Project.dfproj from pak: {}", configRes.error());
				}
			}
		}

		// 3. Fallback for standalone packaged game
		if (!configLoaded)
		{
			if (std::filesystem::exists(exeDir / (exeStem + ".dll")) || std::filesystem::exists(exeDir / "Game.pak"))
			{
				ctx.log.info("Player", "Using packaged standalone configuration for '{}'", exeStem);
				config.name = exeStem;
				if (std::filesystem::exists(exeDir / "Scenes/SponzaAtrium.scene"))
				{
					config.startScene = "Scenes/SponzaAtrium.scene";
				}
				else if (std::filesystem::exists(exeDir / "Scenes/Playground.scene"))
				{
					config.startScene = "Scenes/Playground.scene";
				}
				else
				{
					std::error_code ec;
					for (const auto& entry : std::filesystem::directory_iterator(exeDir / "Scenes", ec))
					{
						if (entry.is_regular_file() && entry.path().extension() == ".scene")
						{
							config.startScene = "Scenes/" + entry.path().filename().string();
							break;
						}
					}
					if (config.startScene.empty())
					{
						config.startScene = "Scenes/Playground.scene";
					}
				}
				config.modulePath = (exeDir / (exeStem + ".dll")).string();
				configLoaded = true;
			}
			else if (std::filesystem::exists("Templates/Playground3D/Project.dfproj"))
			{
				auto configRes = Runtime::ProjectConfig::Load("Templates/Playground3D/Project.dfproj");
				if (configRes.isOk())
				{
					config = configRes.value();
					configLoaded = true;
					projectDir = "Templates/Playground3D";
				}
			}
		}

		if (!configLoaded)
		{
			ctx.log.error("Player", "No project file or pak configuration found.");
			return 1;
		}

		// Fallback unpacked VFS mounts
		if (std::filesystem::exists("Engine/Content"))
		{
			ctx.vfs.mount("/engine/Content", "Engine/Content");
			ctx.vfs.mount("/engine", "Engine/Content");
		}
		else if (std::filesystem::exists("Engine"))
		{
			ctx.vfs.mount("/engine", "Engine");
		}
		if (!projectDir.empty())
		{
			ctx.vfs.mount("", projectDir);
		}

		// Initialize AssetManager for loose development projects (skip in packaged distributions)
		bool isPackaged = std::filesystem::exists(exeDir / "Game.pak") || std::filesystem::exists("Game.pak");
		if (!isPackaged && !projectDir.empty() && std::filesystem::exists(projectDir / config.assetDirectory))
		{
			Core::AssetManager::Get().init(projectDir);
		}

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
		if (std::filesystem::exists(exeDir / (config.name + ".dll")))
		{
			modulePath = exeDir / (config.name + ".dll");
		}
		else if (std::filesystem::exists(exeDir / (exeStem + ".dll")))
		{
			modulePath = exeDir / (exeStem + ".dll");
		}
		else if (!std::filesystem::exists(modulePath))
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
		std::string startSceneToLoad = overrideScene.empty() ? config.startScene : overrideScene;
		std::filesystem::path scenePath = projectDir / startSceneToLoad;
		std::string vscenePath = "/" + startSceneToLoad;
		if (std::filesystem::exists(scenePath) || ctx.vfs.exists(vscenePath) || ctx.vfs.exists("/Scenes/" + scenePath.filename().string()))
		{
			ctx.log.info("Player", "Loading start scene: {}", startSceneToLoad);
			sceneLayer->loadScene(scenePath);
		}
		else
		{
			ctx.log.warn("Player", "Start scene '{}' not found. Running with empty scene.", scenePath.string());
		}

		if (overrideDebugView >= 0 && sceneLayer->getScene())
		{
			auto ppView = sceneLayer->getScene()->view<Core::PostProcessingComponent>();
			for (auto e : ppView)
			{
				ppView.get<Core::PostProcessingComponent>(e).debugView = static_cast<uint32_t>(overrideDebugView);
			}
		}

		app.pushLayer(sceneLayer);

		if (maxFrames > 0)
		{
			ctx.log.info("Player", "Running in bounded frame test mode (maxFrames = {})...", maxFrames);
			app.run(maxFrames);
			if (!screenshotPath.empty())
			{
				app.getRenderer().saveScreenshot(screenshotPath, app.getSwapChain());
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
	catch (const std::exception& e)
	{
		ctx.log.fatal("Player", "Unhandled exception caught: {}", e.what());
		return 1;
	}
	catch (...)
	{
		ctx.log.fatal("Player", "Unknown unhandled exception caught.");
		return 1;
	}
}
