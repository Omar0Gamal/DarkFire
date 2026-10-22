#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"

namespace Core::Systems
{
	class DF_API ParticleSystem
	{
	public:
		static void Tick(Scene& scene, float dt);
		static void Reset(Scene& scene);
	};
}
