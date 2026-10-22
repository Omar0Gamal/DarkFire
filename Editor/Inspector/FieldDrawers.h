#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Reflection/Reflection.h"
#include "Engine/Core/Math/Math.h"
#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
	// Renders standard labeled 3-axis vector control with colored X/Y/Z buttons and reset
	bool DrawVec3Control(
		const char* label,
		Core::Vector3& values,
		float resetValue = 0.0f,
		float columnWidth = 100.0f,
		bool* outActivated = nullptr,
		bool* outDeactivated = nullptr,
		bool isScale = false,
		bool uniformScale = false
	);

	// Generic reflection-driven property drawer for a single field
	bool DrawField(const Core::FieldDesc& field, void* componentData);

	// Formats camelCase/PascalCase component and field names into human-readable titles
	// e.g. "MeshRendererComponent" -> "Mesh Renderer", "isKinematic" -> "Is Kinematic"
	std::string FormatDisplayName(std::string_view name);
}
