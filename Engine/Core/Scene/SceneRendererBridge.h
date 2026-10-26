#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Renderer/RenderScene.h"

namespace Core
{
	DF_API void ExtractRenderScene(
		const Scene& scene,
		Renderer::RenderScene& outRenderScene,
		float targetAspect = 16.0f / 9.0f,
		float alpha = 1.0f
	);
}
