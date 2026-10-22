#include "MeshPrimitives.h"
#include "Engine/Core/Log/Log.h"

#pragma warning(push)
#pragma warning(disable: 4702 4267 4244 4189)
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#pragma warning(pop)

#include <cmath>
#include <filesystem>

namespace Core::Renderer
{
	static constexpr float M_PI_F = 3.14159265358979323846f;

	MeshData CreateCubeMesh(float size)
	{
		MeshData data;
		const float h = size * 0.5f;

		// 6 faces * 4 vertices = 24 vertices
		// Front face (+Z)
		data.vertices.push_back({ { -h, -h,  h }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  h, -h,  h }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  h,  h,  h }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ { -h,  h,  h }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });

		// Back face (-Z)
		data.vertices.push_back({ {  h, -h, -h }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ { -h, -h, -h }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ { -h,  h, -h }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  h,  h, -h }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f } });

		// Top face (+Y)
		data.vertices.push_back({ { -h,  h,  h }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  h,  h,  h }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  h,  h, -h }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ { -h,  h, -h }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });

		// Bottom face (-Y)
		data.vertices.push_back({ { -h, -h, -h }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  h, -h, -h }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  h, -h,  h }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ { -h, -h,  h }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });

		// Right face (+X)
		data.vertices.push_back({ {  h, -h,  h }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f, 1.0f } });
		data.vertices.push_back({ {  h, -h, -h }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f, 1.0f } });
		data.vertices.push_back({ {  h,  h, -h }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f } });
		data.vertices.push_back({ {  h,  h,  h }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f } });

		// Left face (-X)
		data.vertices.push_back({ { -h, -h, -h }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } });
		data.vertices.push_back({ { -h, -h,  h }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } });
		data.vertices.push_back({ { -h,  h,  h }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } });
		data.vertices.push_back({ { -h,  h, -h }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } });

		data.indices.reserve(36);
		for (uint32_t i = 0; i < 6; ++i)
		{
			const uint32_t base = i * 4;
			data.indices.push_back(base + 0);
			data.indices.push_back(base + 1);
			data.indices.push_back(base + 2);
			data.indices.push_back(base + 0);
			data.indices.push_back(base + 2);
			data.indices.push_back(base + 3);
		}

		return data;
	}

	MeshData CreatePlaneMesh(float width, float depth)
	{
		MeshData data;
		const float hw = width * 0.5f;
		const float hd = depth * 0.5f;

		data.vertices.push_back({ { -hw, 0.0f, -hd }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ { -hw, 0.0f,  hd }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  hw, 0.0f,  hd }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		data.vertices.push_back({ {  hw, 0.0f, -hd }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });

		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	MeshData CreateSphereMesh(float radius, uint32_t rings, uint32_t slices)
	{
		MeshData data;

		for (uint32_t r = 0; r <= rings; ++r)
		{
			const float phi = static_cast<float>(r) * M_PI_F / static_cast<float>(rings);
			const float sinPhi = std::sin(phi);
			const float cosPhi = std::cos(phi);

			for (uint32_t s = 0; s <= slices; ++s)
			{
				const float theta = static_cast<float>(s) * 2.0f * M_PI_F / static_cast<float>(slices);
				const float sinTheta = std::sin(theta);
				const float cosTheta = std::cos(theta);

				Math::Vector3 norm(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
				Math::Vector3 pos = norm * radius;
				Math::Vector2 uv(static_cast<float>(s) / slices, static_cast<float>(r) / rings);
				Math::Vector4 tan(-sinTheta, 0.0f, cosTheta, 1.0f);

				data.vertices.push_back({ pos, norm, uv, tan });
			}
		}

		for (uint32_t r = 0; r < rings; ++r)
		{
			for (uint32_t s = 0; s < slices; ++s)
			{
				const uint32_t i0 = r * (slices + 1) + s;
				const uint32_t i1 = (r + 1) * (slices + 1) + s;
				const uint32_t i2 = (r + 1) * (slices + 1) + (s + 1);
				const uint32_t i3 = r * (slices + 1) + (s + 1);

				data.indices.push_back(i0);
				data.indices.push_back(i1);
				data.indices.push_back(i2);

				data.indices.push_back(i0);
				data.indices.push_back(i2);
				data.indices.push_back(i3);
			}
		}

		return data;
	}

	MeshData LoadObjMesh(const std::string& filePath)
	{
		if (filePath == "builtin:cube" || filePath == "cube" || filePath == "cube.obj" ||
			filePath == "/engine/Content/cube.obj" || filePath == "Engine/Content/cube.obj")
		{
			return CreateCubeMesh(1.0f);
		}
		if (filePath == "builtin:sphere" || filePath == "sphere" || filePath == "sphere.obj" ||
			filePath == "/engine/Content/sphere.obj" || filePath == "Engine/Content/sphere.obj")
		{
			return CreateSphereMesh(0.5f, 32, 16);
		}
		if (filePath == "builtin:plane" || filePath == "plane" || filePath == "plane.obj" ||
			filePath == "/engine/Content/plane.obj" || filePath == "Engine/Content/plane.obj")
		{
			return CreatePlaneMesh(10.0f, 10.0f);
		}

		std::string resolvedPath = filePath;
		if (!std::filesystem::exists(resolvedPath))
		{
			std::string stripped = filePath;
			if (stripped.starts_with("/engine/Content/")) stripped = stripped.substr(16);
			else if (stripped.starts_with("/engine/")) stripped = stripped.substr(8);
			else if (stripped.starts_with("engine/Content/")) stripped = stripped.substr(15);
			else if (stripped.starts_with("engine/")) stripped = stripped.substr(7);

			if (std::filesystem::exists("Engine/Content/" + stripped))
			{
				resolvedPath = "Engine/Content/" + stripped;
			}
			else if (std::filesystem::exists("Engine/Content/" + std::filesystem::path(filePath).filename().string()))
			{
				resolvedPath = "Engine/Content/" + std::filesystem::path(filePath).filename().string();
			}
			else if (std::filesystem::exists("Assets/" + filePath))
			{
				resolvedPath = "Assets/" + filePath;
			}
			else
			{
				DF_LOG_WARN("Renderer", "Obj file not found: {}, falling back to procedural primitive", filePath);
				if (filePath.find("sphere") != std::string::npos) return CreateSphereMesh(0.5f, 32, 16);
				if (filePath.find("plane") != std::string::npos) return CreatePlaneMesh(10.0f, 10.0f);
				return CreateCubeMesh(1.0f);
			}
		}

		tinyobj::ObjReaderConfig readerConfig;
		readerConfig.triangulate = true;
		tinyobj::ObjReader reader;

		if (!reader.ParseFromFile(resolvedPath, readerConfig))
		{
			if (!reader.Error().empty())
			{
				DF_LOG_ERROR("Renderer", "TinyObjReader error: {}", reader.Error());
			}
			return CreateCubeMesh(1.0f);
		}

		const auto& attrib = reader.GetAttrib();
		const auto& shapes = reader.GetShapes();

		MeshData data;

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				VertexMesh v{};

				if (index.vertex_index >= 0)
				{
					v.position = Math::Vector3(
						attrib.vertices[3 * index.vertex_index + 0],
						attrib.vertices[3 * index.vertex_index + 1],
						attrib.vertices[3 * index.vertex_index + 2]);
				}

				if (index.normal_index >= 0)
				{
					v.normal = Math::Vector3(
						attrib.normals[3 * index.normal_index + 0],
						attrib.normals[3 * index.normal_index + 1],
						attrib.normals[3 * index.normal_index + 2]);
				}
				else
				{
					v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
				}

				if (index.texcoord_index >= 0)
				{
					v.texcoord = Math::Vector2(
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);
				}
				else
				{
					v.texcoord = Math::Vector2(0.0f, 0.0f);
				}

				v.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
				v.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

				data.indices.push_back(static_cast<uint32_t>(data.vertices.size()));
				data.vertices.push_back(v);
			}
		}

		if (data.vertices.empty())
		{
			return CreateCubeMesh(1.0f);
		}

		return data;
	}
}
