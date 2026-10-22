#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include <vector>
#include <string>

namespace Core::Renderer
{
	struct VertexMesh
	{
		Math::Vector3 position;
		Math::Vector3 normal;
		Math::Vector2 texcoord;
		Math::Vector4 tangent;
		Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	};

	struct MeshData
	{
		std::vector<VertexMesh> vertices;
		std::vector<uint32_t> indices;
	};

	DF_API MeshData CreateCubeMesh(float size = 1.0f);
	DF_API MeshData CreateSphereMesh(float radius = 0.5f, uint32_t rings = 16, uint32_t slices = 32);
	DF_API MeshData CreatePlaneMesh(float width = 1.0f, float depth = 1.0f);
	DF_API MeshData LoadObjMesh(const std::string& filePath);
}
