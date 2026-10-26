#include "MeshPrimitives.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/VFS/AssetPathResolver.h"

#pragma warning(push)
#pragma warning(disable: 4702 4267 4244 4189)
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#pragma warning(pop)

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace Core::Renderer
{
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

		auto resolved = Core::AssetPathResolver::resolve(filePath);
		if (!resolved.has_value() || resolved->isVFS)
		{
			DF_LOG_WARN("Renderer", "Obj file not found: {}, falling back to procedural primitive", filePath);
			if (filePath.find("sphere") != std::string::npos) return CreateSphereMesh(0.5f, 32, 16);
			if (filePath.find("plane") != std::string::npos) return CreatePlaneMesh(10.0f, 10.0f);
			return CreateCubeMesh(1.0f);
		}

		std::string resolvedPath = resolved->physicalPath.string();

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

		struct IndexKey
		{
			int v, n, t;
			bool operator==(const IndexKey& o) const noexcept { return v == o.v && n == o.n && t == o.t; }
		};

		struct IndexKeyHash
		{
			size_t operator()(const IndexKey& k) const noexcept
			{
				size_t h = std::hash<int>{}(k.v);
				h ^= std::hash<int>{}(k.n) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<int>{}(k.t) + 0x9e3779b9 + (h << 6) + (h >> 2);
				return h;
			}
		};

		std::unordered_map<IndexKey, uint32_t, IndexKeyHash> uniqueVertices;
		bool hasNormalsInFile = !attrib.normals.empty();

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				IndexKey key{ index.vertex_index, index.normal_index, index.texcoord_index };
				auto it = uniqueVertices.find(key);
				if (it != uniqueVertices.end())
				{
					data.indices.push_back(it->second);
				}
				else
				{
					VertexMesh v{};

					if (index.vertex_index >= 0)
					{
						v.position = Math::Vector3(
							attrib.vertices[3 * index.vertex_index + 0],
							attrib.vertices[3 * index.vertex_index + 1],
							attrib.vertices[3 * index.vertex_index + 2]);
					}

					if (index.normal_index >= 0 && hasNormalsInFile)
					{
						v.normal = Math::Vector3(
							attrib.normals[3 * index.normal_index + 0],
							attrib.normals[3 * index.normal_index + 1],
							attrib.normals[3 * index.normal_index + 2]);
					}
					else
					{
						v.normal = Math::Vector3(0.0f, 0.0f, 0.0f); // Will be calculated below if missing
					}

					if (index.texcoord_index >= 0 && !attrib.texcoords.empty())
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

					uint32_t newIndex = static_cast<uint32_t>(data.vertices.size());
					data.vertices.push_back(v);
					data.indices.push_back(newIndex);
					uniqueVertices[key] = newIndex;
				}
			}
		}

		if (data.vertices.empty())
		{
			return CreateCubeMesh(1.0f);
		}

		// 1. If normals were missing from OBJ file, generate area-weighted smooth normals
		if (!hasNormalsInFile)
		{
			const size_t triangleCount = data.indices.size() / 3;
			for (size_t t = 0; t < triangleCount; ++t)
			{
				uint32_t i0 = data.indices[t * 3 + 0];
				uint32_t i1 = data.indices[t * 3 + 1];
				uint32_t i2 = data.indices[t * 3 + 2];

				Math::Vector3 e1 = data.vertices[i1].position - data.vertices[i0].position;
				Math::Vector3 e2 = data.vertices[i2].position - data.vertices[i0].position;
				Math::Vector3 crossN = e1.Cross(e2); // Area-weighted normal vector

				data.vertices[i0].normal += crossN;
				data.vertices[i1].normal += crossN;
				data.vertices[i2].normal += crossN;
			}

			for (auto& vert : data.vertices)
			{
				if (vert.normal.LengthSquared() > 1e-6f)
				{
					vert.normal.Normalize();
				}
				else
				{
					vert.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
				}
			}
		}

		// 2. Lengyel's Tangent and Bitangent accumulation
		std::vector<Math::Vector3> tan1(data.vertices.size(), Math::Vector3(0.0f, 0.0f, 0.0f));
		std::vector<Math::Vector3> tan2(data.vertices.size(), Math::Vector3(0.0f, 0.0f, 0.0f));

		const size_t triCount = data.indices.size() / 3;
		for (size_t t = 0; t < triCount; ++t)
		{
			uint32_t i0 = data.indices[t * 3 + 0];
			uint32_t i1 = data.indices[t * 3 + 1];
			uint32_t i2 = data.indices[t * 3 + 2];

			const auto& v0 = data.vertices[i0];
			const auto& v1 = data.vertices[i1];
			const auto& v2 = data.vertices[i2];

			Math::Vector3 e1 = v1.position - v0.position;
			Math::Vector3 e2 = v2.position - v0.position;

			float du1 = v1.texcoord.x - v0.texcoord.x;
			float dv1 = v1.texcoord.y - v0.texcoord.y;
			float du2 = v2.texcoord.x - v0.texcoord.x;
			float dv2 = v2.texcoord.y - v0.texcoord.y;

			float det = du1 * dv2 - du2 * dv1;
			if (std::abs(det) > 1e-7f)
			{
				float r = 1.0f / det;
				Math::Vector3 sdir = (e1 * dv2 - e2 * dv1) * r;
				Math::Vector3 tdir = (e2 * du1 - e1 * du2) * r;

				tan1[i0] += sdir;
				tan1[i1] += sdir;
				tan1[i2] += sdir;

				tan2[i0] += tdir;
				tan2[i1] += tdir;
				tan2[i2] += tdir;
			}
		}

		// 3. Gram-Schmidt Orthogonalization & Handedness Calculation
		for (size_t i = 0; i < data.vertices.size(); ++i)
		{
			auto& vert = data.vertices[i];
			const Math::Vector3& n = vert.normal;
			const Math::Vector3& t = tan1[i];
			const Math::Vector3& b = tan2[i];

			if (t.LengthSquared() < 1e-6f)
			{
				// Degenerate UVs: fallback to arbitrary orthonormal basis
				Math::Vector3 arbT = (std::abs(n.y) < 0.99f) ? n.Cross(Math::Vector3(0.0f, 1.0f, 0.0f)) : n.Cross(Math::Vector3(1.0f, 0.0f, 0.0f));
				arbT.Normalize();
				vert.tangent = Math::Vector4(arbT.x, arbT.y, arbT.z, 1.0f);
			}
			else
			{
				// Gram-Schmidt orthogonalize: t' = normalize(t - n * dot(n, t))
				Math::Vector3 orthoT = t - n * n.Dot(t);
				if (orthoT.LengthSquared() > 1e-6f)
				{
					orthoT.Normalize();
				}
				else
				{
					orthoT = (std::abs(n.y) < 0.99f) ? n.Cross(Math::Vector3(0.0f, 1.0f, 0.0f)) : n.Cross(Math::Vector3(1.0f, 0.0f, 0.0f));
					orthoT.Normalize();
				}

				// Calculate handedness: w = sign(dot(cross(n, orthoT), b))
				float handedness = (n.Cross(orthoT).Dot(b) < 0.0f) ? -1.0f : 1.0f;
				vert.tangent = Math::Vector4(orthoT.x, orthoT.y, orthoT.z, handedness);
			}
		}

		ComputeLocalBounds(data);
		return data;
	}
}
