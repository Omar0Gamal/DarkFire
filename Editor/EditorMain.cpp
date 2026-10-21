#include "Engine/Runtime/Application.h"
#include "Engine/Platform/CrashHandler.h"
#include "Editor/EditorLayer.h"
#include <iostream>

int main(int argc, char** argv)
{
	Platform::CrashHandler::Install();

	Runtime::ProjectConfig config;
	config.name = "DarkFire Editor";
	config.windowTitle = "DarkFire Engine - Standalone Editor";
	config.windowWidth = 1600;
	config.windowHeight = 900;
	config.enableImGui = true;

	// Check CLI arguments: Editor.exe --project <path> or Editor.exe <scene>
	std::filesystem::path initialScenePath;
	for (int i = 1; i < argc; ++i)
	{
		std::string_view arg = argv[i];
		if ((arg == "--project" || arg == "-p") && i + 1 < argc)
		{
			std::filesystem::path projPath = argv[++i];
			auto projRes = Runtime::ProjectConfig::Load(projPath);
			if (projRes.isOk())
			{
				config = projRes.value();
				config.windowTitle = "DarkFire Engine - " + config.name;
				config.enableImGui = true;
			}
		}
		else if (arg.ends_with(".scene"))
		{
			initialScenePath = arg;
		}
	}

	Runtime::Application app(config);
	if (!app.init())
	{
		return 1;
	}

	auto editorLayer = std::make_shared<Editor::EditorLayer>();
	if (!initialScenePath.empty())
	{
		editorLayer->openScene(initialScenePath);
	}

	app.pushLayer(editorLayer);
	app.run();

	return 0;
}
