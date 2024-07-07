#pragma once

#include "Core/Game/IComponent.h"
#include "Core/Math/Vector3d.h"
namespace Core
{
	class LightComponent : public IComponent
	{
	public:
		LightComponent(Vector3d color, float intensity)
			: m_Color(color), m_Intensity(intensity) {}
	public:
		Vector3d m_Color;
		float m_Intensity;
	};
}