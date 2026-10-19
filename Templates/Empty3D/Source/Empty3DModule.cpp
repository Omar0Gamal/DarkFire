#include "Engine/Runtime/Module.h"

extern "C"
{
	DF_MODULE_API Core::ModuleInfo DF_GetModuleInfo()
	{
		return Core::ModuleInfo{
			"Empty3D",
			"1.0.0",
			Core::DARKFIRE_API_VERSION
		};
	}

	DF_MODULE_API void DF_RegisterTypes(Core::EngineContext& ctx)
	{
		ctx.log.info("Empty3D", "Empty3D module type registration.");
	}

	DF_MODULE_API void DF_OnLoad(Core::EngineContext& ctx)
	{
		ctx.log.info("Empty3D", "Empty3D module loaded successfully.");
	}

	DF_MODULE_API void DF_OnUnload()
	{
		Core::Log::Get().info("Empty3D", "Empty3D module unloaded.");
	}

	DF_MODULE_API void DF_RegisterEditorTools(Core::EditorContext& ctx)
	{
		(void)ctx;
	}
}
