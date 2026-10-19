#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Runtime/EngineContext.h"
#include <cstdint>

namespace Core
{
	struct ModuleInfo
	{
		const char* name;
		const char* version;
		uint32_t apiVersion;
	};

	inline constexpr uint32_t DARKFIRE_API_VERSION = 200; // v2.0
}

// Module boundary function pointer types
using PFN_DF_GetModuleInfo = Core::ModuleInfo (*)();
using PFN_DF_RegisterTypes = void (*)(Core::EngineContext&);
using PFN_DF_OnLoad = void (*)(Core::EngineContext&);
using PFN_DF_OnUnload = void (*)();
using PFN_DF_RegisterEditorTools = void (*)(Core::EditorContext&);
