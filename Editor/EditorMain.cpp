#include "Engine/Runtime/EngineContext.h"
#include "Engine/Platform/CrashHandler.h"
#include "imgui.h"
#include <iostream>

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	Platform::CrashHandler::Install();

	auto& ctx = Core::EngineContext::Get();
	ctx.log.info("Editor", "DarkFire Standalone Editor (Editor.exe) stub initialized.");
	ctx.log.info("Editor", "Dear ImGui version: {}", IMGUI_VERSION);

	return 0;
}
