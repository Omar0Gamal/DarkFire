#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Renderer/RenderScene.h"

namespace Core
{
	DF_API void ExtractRenderScene(const Scene& scene, Renderer::RenderScene& outRenderScene);
}
