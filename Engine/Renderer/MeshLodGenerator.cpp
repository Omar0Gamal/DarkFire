#include "MeshPrimitives.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Core::Renderer
{
	void GenerateMeshLODs(MeshData& data)
	{
		data.lodLevels.clear();
		if (data.vertices.empty() || data.indices.empty())
		{
			return;
		}

		// Compute overall bounds if not yet populated
		if (data.localBounds.Extents.x <= 0.0f && data.localBounds.Extents.y <= 0.0f && data.localBounds.Extents.z <= 0.0f)
		{
			ComputeLocalBounds(data);
		}

		// LOD 0: Full Detail
		MeshLodLevel lod0{};
		lod0.startIndexLocation = 0;
		lod0.indexCount = static_cast<uint32_t>(data.indices.size());
		lod0.submeshes = data.submeshes;
		lod0.maxDistance = 30.0f;
		if (lod0.submeshes.empty())
		{
			Submesh single{};
			single.startIndexLocation = 0;
			single.indexCount = lod0.indexCount;
			single.baseVertexLocation = 0;
			single.localBounds = data.localBounds;
			lod0.submeshes.push_back(single);
		}
		data.lodLevels.push_back(lod0);

		// Only generate simplified LOD1 and LOD2 if mesh has enough triangles (> 300 indices)
		if (lod0.indexCount <= 300)
		{
			return;
		}

		const uint32_t gridResolutions[2] = { 48u, 24u };
		const float maxDistances[2] = { 75.0f, 10000.0f };
		data.indices.reserve(static_cast<size_t>(lod0.indexCount) * 3);

		for (int lodIdx = 0; lodIdx < 2; ++lodIdx)
		{
			uint32_t gridRes = gridResolutions[lodIdx];
			MeshLodLevel lod{};
			lod.startIndexLocation = static_cast<uint32_t>(data.indices.size());
			lod.maxDistance = maxDistances[lodIdx];

			for (const auto& srcSub : lod0.submeshes)
			{
				Submesh dstSub = srcSub;
				dstSub.startIndexLocation = static_cast<uint32_t>(data.indices.size());

				// Keep small submeshes or alpha-cutout foliage/chains intact across all LODs
				if (srcSub.indexCount <= 192 || srcSub.isAlphaCutout)
				{
					for (uint32_t k = 0; k < srcSub.indexCount; ++k)
					{
						data.indices.push_back(data.indices[srcSub.startIndexLocation + k]);
					}
					dstSub.indexCount = srcSub.indexCount;
					lod.submeshes.push_back(dstSub);
					continue;
				}

				Math::Vector3 sMin(
					srcSub.localBounds.Center.x - srcSub.localBounds.Extents.x,
					srcSub.localBounds.Center.y - srcSub.localBounds.Extents.y,
					srcSub.localBounds.Center.z - srcSub.localBounds.Extents.z);
				Math::Vector3 sExtent(
					std::max(srcSub.localBounds.Extents.x * 2.0f, 0.001f),
					std::max(srcSub.localBounds.Extents.y * 2.0f, 0.001f),
					std::max(srcSub.localBounds.Extents.z * 2.0f, 0.001f));

				std::unordered_map<uint64_t, uint32_t> cellToVertex;
				cellToVertex.reserve(srcSub.indexCount / 2);

				auto quantizeVertex = [&](uint32_t vIdx) -> uint32_t {
					const auto& v = data.vertices[vIdx];
					uint64_t gx = static_cast<uint64_t>(std::clamp(static_cast<int>((v.position.x - sMin.x) / sExtent.x * gridRes), 0, static_cast<int>(gridRes - 1)));
					uint64_t gy = static_cast<uint64_t>(std::clamp(static_cast<int>((v.position.y - sMin.y) / sExtent.y * gridRes), 0, static_cast<int>(gridRes - 1)));
					uint64_t gz = static_cast<uint64_t>(std::clamp(static_cast<int>((v.position.z - sMin.z) / sExtent.z * gridRes), 0, static_cast<int>(gridRes - 1)));
					uint64_t nOct = (v.normal.x >= 0.0f ? 1ULL : 0ULL) | (v.normal.y >= 0.0f ? 2ULL : 0ULL) | (v.normal.z >= 0.0f ? 4ULL : 0ULL);
					uint64_t key = (gx) | (gy << 12) | (gz << 24) | (nOct << 36);
					auto [it, inserted] = cellToVertex.emplace(key, vIdx);
					return it->second;
				};

				std::vector<uint32_t> simplified;
				simplified.reserve(srcSub.indexCount);
				uint32_t triCount = srcSub.indexCount / 3;
				for (uint32_t t = 0; t < triCount; ++t)
				{
					uint32_t i0 = data.indices[srcSub.startIndexLocation + t * 3 + 0];
					uint32_t i1 = data.indices[srcSub.startIndexLocation + t * 3 + 1];
					uint32_t i2 = data.indices[srcSub.startIndexLocation + t * 3 + 2];

					uint32_t r0 = quantizeVertex(i0);
					uint32_t r1 = quantizeVertex(i1);
					uint32_t r2 = quantizeVertex(i2);

					if (r0 != r1 && r1 != r2 && r0 != r2)
					{
						simplified.push_back(r0);
						simplified.push_back(r1);
						simplified.push_back(r2);
					}
				}

				if (simplified.size() < 36)
				{
					for (uint32_t k = 0; k < srcSub.indexCount; ++k)
					{
						data.indices.push_back(data.indices[srcSub.startIndexLocation + k]);
					}
					dstSub.indexCount = srcSub.indexCount;
				}
				else
				{
					data.indices.insert(data.indices.end(), simplified.begin(), simplified.end());
					dstSub.indexCount = static_cast<uint32_t>(simplified.size());
				}
				lod.submeshes.push_back(dstSub);
			}

			lod.indexCount = static_cast<uint32_t>(data.indices.size()) - lod.startIndexLocation;
			data.lodLevels.push_back(std::move(lod));
		}
	}
}
