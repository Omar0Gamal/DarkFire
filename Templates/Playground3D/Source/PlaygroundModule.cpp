#include "Engine/Runtime/Module.h"

extern "C"
{
	DF_MODULE_API Core::ModuleInfo DF_GetModuleInfo()
	{
		return Core::ModuleInfo{
			"Playground3D",
			"1.0.0",
			Core::DARKFIRE_API_VERSION
		};
	}

	DF_MODULE_API void DF_RegisterTypes(Core::EngineContext& ctx)
	{
		ctx.log.info("Playground3D", "Registering Playground3D types into ComponentRegistry...");
	}

	DF_MODULE_API void DF_OnLoad(Core::EngineContext& ctx)
	{
		ctx.log.info("Playground3D", "Playground3D module loaded successfully across DLL boundary!");
	}

	DF_MODULE_API void DF_OnUnload()
	{
		Core::Log::Get().info("Playground3D", "Playground3D module unloaded.");
	}

	DF_MODULE_API void DF_RegisterEditorTools(Core::EditorContext& ctx)
	{
		(void)ctx;
	}
}
