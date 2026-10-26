#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include <vector>

namespace Core::Renderer
{
	struct DebugVertex
	{
		Math::Vector3 pos;
		Math::Vector4 color;
	};

	class DF_API DebugRenderer
	{
	public:
		static void drawLine(const Math::Vector3& p0, const Math::Vector3& p1, const Math::Vector4& color);
		static void drawBox(const Math::Vector3& center, const Math::Vector3& size, const Math::Quaternion& rotation, const Math::Vector4& color);
		static void drawSphere(const Math::Vector3& center, float radius, const Math::Vector4& color, uint32_t segments = 16);
		static void clear();
		static const std::vector<DebugVertex>& getVertices();
	};
}
