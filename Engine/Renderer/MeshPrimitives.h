#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include <vector>
#include <string>
#include <filesystem>

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

	struct Submesh
	{
		uint32_t startIndexLocation = 0;
		uint32_t indexCount = 0;
		int32_t baseVertexLocation = 0;
		Math::BoundingBox localBounds{};
		std::string name;
		std::string defaultMaterialPath;
		bool isAlphaCutout = false;
	};

	struct MeshLodLevel
	{
		uint32_t startIndexLocation = 0;
		uint32_t indexCount = 0;
		std::vector<Submesh> submeshes;
		float maxDistance = 1000.0f;
	};

	struct MeshData
	{
		std::vector<VertexMesh> vertices;
		std::vector<uint32_t> indices;
		std::vector<Submesh> submeshes;
		std::vector<MeshLodLevel> lodLevels;
		Math::BoundingBox localBounds{};
		uint32_t shadowOpaqueStartIndex = 0;
		uint32_t shadowOpaqueIndexCount = 0;
	};

	DF_API void ComputeLocalBounds(MeshData& data);
	DF_API void GenerateMeshLODs(MeshData& data);
	DF_API MeshData CreateCubeMesh(float size = 1.0f);
	DF_API MeshData CreateSphereMesh(float radius = 0.5f, uint32_t rings = 16, uint32_t slices = 32);
	DF_API MeshData CreatePlaneMesh(float width = 1.0f, float depth = 1.0f);
	DF_API MeshData CreateGrassBladeMesh(uint32_t segments = 5);
	DF_API MeshData CreateSubdividedPlaneMesh(float size, uint32_t resolution);
	DF_API MeshData LoadObjMesh(const std::string& filePath);
	DF_API MeshData LoadFbxMesh(const std::string& filePath);
	DF_API MeshData LoadMesh(const std::string& filePath);

	DF_API bool SaveBinaryMesh(const std::filesystem::path& filePath, const MeshData& meshData);
	DF_API bool LoadBinaryMeshFromMemory(const uint8_t* data, size_t size, MeshData& outMeshData);
	DF_API bool LoadBinaryMesh(const std::string& virtualOrDiskPath, MeshData& outMeshData);
}
