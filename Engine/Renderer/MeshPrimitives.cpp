#include "MeshPrimitives.h"
#include "Engine/Core/VFS/AssetPathResolver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Core::Renderer
{
	static constexpr float M_PI_F = 3.14159265358979323846f;

	static std::string ToLower(std::string_view s)
	{
		std::string result;
		result.reserve(s.size());
		for (char c : s)
		{
			result.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
		}
		return result;
	}

	void ComputeLocalBounds(MeshData& data)
	{
		if (data.vertices.empty())
		{
			data.localBounds = Math::BoundingBox();
			return;
		}

		Math::Vector3 bMin(1e30f, 1e30f, 1e30f);
		Math::Vector3 bMax(-1e30f, -1e30f, -1e30f);
		for (const auto& v : data.vertices)
		{
			bMin = Math::Vector3::Min(bMin, v.position);
			bMax = Math::Vector3::Max(bMax, v.position);
		}
		data.localBounds.Center = (bMin + bMax) * 0.5f;
		data.localBounds.Extents = (bMax - bMin) * 0.5f;
	}

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

		ComputeLocalBounds(data);
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

		ComputeLocalBounds(data);
		return data;
	}

	MeshData CreateGrassBladeMesh(uint32_t segments)
	{
		MeshData data;
		segments = std::max(segments, 1u);

		data.vertices.reserve((segments + 1) * 2);
		data.indices.reserve(segments * 12);

		for (uint32_t i = 0; i <= segments; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(segments);
			const float y = t;
			const float w = (1.0f - t * 0.85f) * 0.5f;
			const float z = t * t * 0.15f;

			Math::Vector3 normal(0.0f, -t * 0.2f, 1.0f);
			normal.Normalize();

			VertexMesh vL{};
			vL.position = Math::Vector3(-w, y, z);
			vL.normal = normal;
			vL.texcoord = Math::Vector2(0.0f, t);
			vL.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
			vL.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			data.vertices.push_back(vL);

			VertexMesh vR{};
			vR.position = Math::Vector3(w, y, z);
			vR.normal = normal;
			vR.texcoord = Math::Vector2(1.0f, t);
			vR.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
			vR.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			data.vertices.push_back(vR);
		}

		for (uint32_t i = 0; i < segments; ++i)
		{
			uint32_t i0 = i * 2;
			uint32_t i1 = i * 2 + 1;
			uint32_t i2 = (i + 1) * 2;
			uint32_t i3 = (i + 1) * 2 + 1;

			// Front face
			data.indices.push_back(i0);
			data.indices.push_back(i2);
			data.indices.push_back(i1);

			data.indices.push_back(i1);
			data.indices.push_back(i2);
			data.indices.push_back(i3);

			// Back face
			data.indices.push_back(i0);
			data.indices.push_back(i1);
			data.indices.push_back(i2);

			data.indices.push_back(i1);
			data.indices.push_back(i3);
			data.indices.push_back(i2);
		}

		ComputeLocalBounds(data);

		Submesh submesh{};
		submesh.startIndexLocation = 0;
		submesh.indexCount = static_cast<uint32_t>(data.indices.size());
		submesh.baseVertexLocation = 0;
		submesh.localBounds = data.localBounds;
		submesh.name = "GrassBlade";
		data.submeshes.push_back(submesh);

		return data;
	}

	MeshData CreateSubdividedPlaneMesh(float size, uint32_t resolution)
	{
		MeshData data;
		resolution = std::max(resolution, 1u);
		const float halfSize = size * 0.5f;
		const float step = size / static_cast<float>(resolution);

		data.vertices.reserve((resolution + 1) * (resolution + 1));
		data.indices.reserve(resolution * resolution * 6);

		for (uint32_t iz = 0; iz <= resolution; ++iz)
		{
			const float z = -halfSize + static_cast<float>(iz) * step;
			const float v = static_cast<float>(iz) / static_cast<float>(resolution);

			for (uint32_t ix = 0; ix <= resolution; ++ix)
			{
				const float x = -halfSize + static_cast<float>(ix) * step;
				const float u = static_cast<float>(ix) / static_cast<float>(resolution);

				VertexMesh vert{};
				vert.position = Math::Vector3(x, 0.0f, z);
				vert.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
				vert.texcoord = Math::Vector2(u, v);
				vert.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
				vert.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
				data.vertices.push_back(vert);
			}
		}

		for (uint32_t iz = 0; iz < resolution; ++iz)
		{
			for (uint32_t ix = 0; ix < resolution; ++ix)
			{
				uint32_t i0 = iz * (resolution + 1) + ix;
				uint32_t i1 = (iz + 1) * (resolution + 1) + ix;
				uint32_t i2 = (iz + 1) * (resolution + 1) + (ix + 1);
				uint32_t i3 = iz * (resolution + 1) + (ix + 1);

				data.indices.push_back(i0);
				data.indices.push_back(i1);
				data.indices.push_back(i2);

				data.indices.push_back(i0);
				data.indices.push_back(i2);
				data.indices.push_back(i3);
			}
		}

		ComputeLocalBounds(data);

		Submesh submesh{};
		submesh.startIndexLocation = 0;
		submesh.indexCount = static_cast<uint32_t>(data.indices.size());
		submesh.baseVertexLocation = 0;
		submesh.localBounds = data.localBounds;
		submesh.name = "SubdividedPlane";
		data.submeshes.push_back(submesh);

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

		ComputeLocalBounds(data);
		return data;
	}

	namespace
	{
		struct BufferWriter
		{
			std::vector<uint8_t> buffer;

			template<typename T>
			void write(const T& val)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&val);
				buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
			}

			void writeBytes(const void* data, size_t size)
			{
				if (size > 0 && data)
				{
					const uint8_t* ptr = static_cast<const uint8_t*>(data);
					buffer.insert(buffer.end(), ptr, ptr + size);
				}
			}

			void writeString(const std::string& str)
			{
				uint32_t len = static_cast<uint32_t>(str.size());
				write(len);
				if (len > 0)
				{
					writeBytes(str.data(), len);
				}
			}

			void writeSubmesh(const Submesh& sub)
			{
				write(sub.startIndexLocation);
				write(sub.indexCount);
				write(sub.baseVertexLocation);
				write(sub.localBounds);
				uint8_t cutout = sub.isAlphaCutout ? 1 : 0;
				write(cutout);
				writeString(sub.name);
				writeString(sub.defaultMaterialPath);
			}
		};

		struct BufferReader
		{
			const uint8_t* data = nullptr;
			size_t size = 0;
			size_t offset = 0;

			[[nodiscard]] bool hasRemaining(size_t bytes) const noexcept
			{
				return offset + bytes <= size;
			}

			template<typename T>
			bool read(T& val)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				if (!hasRemaining(sizeof(T)))
				{
					return false;
				}
				std::memcpy(&val, data + offset, sizeof(T));
				offset += sizeof(T);
				return true;
			}

			bool readBytes(void* dest, size_t bytes)
			{
				if (bytes == 0) return true;
				if (!hasRemaining(bytes)) return false;
				std::memcpy(dest, data + offset, bytes);
				offset += bytes;
				return true;
			}

			bool readString(std::string& str)
			{
				uint32_t len = 0;
				if (!read(len)) return false;
				if (len > 0)
				{
					if (!hasRemaining(len)) return false;
					str.assign(reinterpret_cast<const char*>(data + offset), len);
					offset += len;
				}
				else
				{
					str.clear();
				}
				return true;
			}

			bool readSubmesh(Submesh& sub)
			{
				if (!read(sub.startIndexLocation)) return false;
				if (!read(sub.indexCount)) return false;
				if (!read(sub.baseVertexLocation)) return false;
				if (!read(sub.localBounds)) return false;
				uint8_t cutout = 0;
				if (!read(cutout)) return false;
				sub.isAlphaCutout = (cutout != 0);
				if (!readString(sub.name)) return false;
				if (!readString(sub.defaultMaterialPath)) return false;
				return true;
			}
		};
	}

	bool SaveBinaryMesh(const std::filesystem::path& filePath, const MeshData& meshData)
	{
		BufferWriter writer;

		// Header
		constexpr uint32_t kDfMeshMagic = 0x534D4644; // 'DFMS'
		constexpr uint32_t kDfMeshVersion = 1;

		writer.write(kDfMeshMagic);
		writer.write(kDfMeshVersion);
		writer.write<uint32_t>(static_cast<uint32_t>(meshData.vertices.size()));
		writer.write<uint32_t>(static_cast<uint32_t>(meshData.indices.size()));
		writer.write<uint32_t>(static_cast<uint32_t>(meshData.submeshes.size()));
		writer.write<uint32_t>(static_cast<uint32_t>(meshData.lodLevels.size()));
		writer.write<Math::BoundingBox>(meshData.localBounds);
		writer.write<uint32_t>(meshData.shadowOpaqueStartIndex);
		writer.write<uint32_t>(meshData.shadowOpaqueIndexCount);

		// Contiguous vertices
		if (!meshData.vertices.empty())
		{
			writer.writeBytes(meshData.vertices.data(), meshData.vertices.size() * sizeof(VertexMesh));
		}

		// Contiguous indices
		if (!meshData.indices.empty())
		{
			writer.writeBytes(meshData.indices.data(), meshData.indices.size() * sizeof(uint32_t));
		}

		// Submesh table
		for (const auto& sub : meshData.submeshes)
		{
			writer.writeSubmesh(sub);
		}

		// LOD table
		for (const auto& lod : meshData.lodLevels)
		{
			writer.write(lod.startIndexLocation);
			writer.write(lod.indexCount);
			writer.write(lod.maxDistance);
			writer.write<uint32_t>(static_cast<uint32_t>(lod.submeshes.size()));
			for (const auto& sub : lod.submeshes)
			{
				writer.writeSubmesh(sub);
			}
		}

		if (filePath.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::create_directories(filePath.parent_path(), ec);
		}

		std::ofstream out(filePath, std::ios::binary);
		if (!out.is_open())
		{
			return false;
		}

		out.write(reinterpret_cast<const char*>(writer.buffer.data()), writer.buffer.size());
		return out.good();
	}

	bool LoadBinaryMeshFromMemory(const uint8_t* data, size_t size, MeshData& outMeshData)
	{
		if (!data || size < 52)
		{
			return false;
		}

		BufferReader reader{ data, size, 0 };

		uint32_t magic = 0;
		uint32_t version = 0;
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
		uint32_t submeshCount = 0;
		uint32_t lodCount = 0;
		Math::BoundingBox localBounds{};
		uint32_t shadowOpaqueStartIndex = 0;
		uint32_t shadowOpaqueIndexCount = 0;

		if (!reader.read(magic) || magic != 0x534D4644) return false;
		if (!reader.read(version) || version != 1) return false;
		if (!reader.read(vertexCount)) return false;
		if (!reader.read(indexCount)) return false;
		if (!reader.read(submeshCount)) return false;
		if (!reader.read(lodCount)) return false;
		if (!reader.read(localBounds)) return false;
		if (!reader.read(shadowOpaqueStartIndex)) return false;
		if (!reader.read(shadowOpaqueIndexCount)) return false;

		MeshData result{};
		result.localBounds = localBounds;
		result.shadowOpaqueStartIndex = shadowOpaqueStartIndex;
		result.shadowOpaqueIndexCount = shadowOpaqueIndexCount;

		// Contiguous vertices
		result.vertices.resize(vertexCount);
		if (vertexCount > 0)
		{
			if (!reader.readBytes(result.vertices.data(), vertexCount * sizeof(VertexMesh)))
			{
				return false;
			}
		}

		// Contiguous indices
		result.indices.resize(indexCount);
		if (indexCount > 0)
		{
			if (!reader.readBytes(result.indices.data(), indexCount * sizeof(uint32_t)))
			{
				return false;
			}
		}

		// Submesh table
		result.submeshes.resize(submeshCount);
		for (uint32_t i = 0; i < submeshCount; ++i)
		{
			if (!reader.readSubmesh(result.submeshes[i]))
			{
				return false;
			}
		}

		// LOD table
		result.lodLevels.resize(lodCount);
		for (uint32_t i = 0; i < lodCount; ++i)
		{
			auto& lod = result.lodLevels[i];
			if (!reader.read(lod.startIndexLocation)) return false;
			if (!reader.read(lod.indexCount)) return false;
			if (!reader.read(lod.maxDistance)) return false;
			uint32_t lodSubCount = 0;
			if (!reader.read(lodSubCount)) return false;
			lod.submeshes.resize(lodSubCount);
			for (uint32_t s = 0; s < lodSubCount; ++s)
			{
				if (!reader.readSubmesh(lod.submeshes[s]))
				{
					return false;
				}
			}
		}

		outMeshData = std::move(result);
		return true;
	}

	bool LoadBinaryMesh(const std::string& virtualOrDiskPath, MeshData& outMeshData)
	{
		// 1. Try unified VFS -> Physical fallback via AssetPathResolver
		auto binRes = Core::AssetPathResolver::readBinary(virtualOrDiskPath);
		if (binRes.isOk() && !binRes.value().empty())
		{
			return LoadBinaryMeshFromMemory(binRes.value().data(), binRes.value().size(), outMeshData);
		}

		// 2. Direct physical filesystem fallback
		std::filesystem::path p(virtualOrDiskPath);
		std::error_code ec;
		if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec))
		{
			std::ifstream file(p, std::ios::binary | std::ios::ate);
			if (file.is_open())
			{
				std::streamsize fileSize = file.tellg();
				if (fileSize > 0)
				{
					file.seekg(0, std::ios::beg);
					std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
					if (file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
					{
						return LoadBinaryMeshFromMemory(buffer.data(), buffer.size(), outMeshData);
					}
				}
			}
		}

		return false;
	}

	MeshData LoadMesh(const std::string& filePath)
	{
		std::string lower = ToLower(filePath);
		if (lower.ends_with(".dfmesh"))
		{
			MeshData binaryData;
			if (LoadBinaryMesh(filePath, binaryData))
			{
				return binaryData;
			}
		}

		std::filesystem::path p(filePath);
		std::filesystem::path companionPath = p;
		companionPath.replace_extension(".dfmesh");
		std::string companionStr = companionPath.generic_string();

		if (companionStr != filePath)
		{
			MeshData companionData;
			if (LoadBinaryMesh(companionStr, companionData))
			{
				return companionData;
			}
		}

		if (lower.ends_with(".fbx"))
		{
			return LoadFbxMesh(filePath);
		}
		MeshData objData = LoadObjMesh(filePath);
		GenerateMeshLODs(objData);
		return objData;
	}
}
