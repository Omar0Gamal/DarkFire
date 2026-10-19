#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/RHI/IRHI.h"
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace Core::Renderer
{
	enum class RenderLayer : uint8_t
	{
		Background = 0,
		Opaque = 1,
		AlphaTest = 2,
		Transparent = 3,
		Overlay = 4
	};

	struct InstanceData
	{
		Math::Matrix world;
		uint32_t entityId = 0;
		float pad[3] = {};
	};

	struct DrawItem
	{
		uint64_t sortKey = 0;
		uint32_t shaderVariantId = 0;
		uint32_t materialId = 0;
		uint32_t meshId = 0;
		uint32_t indexCount = 0;
		uint32_t startIndexLocation = 0;
		int32_t baseVertexLocation = 0;
		InstanceData instanceData{};

		static uint64_t makeSortKey(
			RenderLayer layer,
			uint32_t shaderVariantId,
			uint32_t materialId,
			uint32_t meshId,
			float depth) noexcept
		{
			const uint64_t l = static_cast<uint64_t>(layer) & 0xFULL;
			const uint64_t s = static_cast<uint64_t>(shaderVariantId) & 0xFFFULL;
			const uint64_t m = static_cast<uint64_t>(materialId) & 0xFFFFULL;
			const uint64_t g = static_cast<uint64_t>(meshId) & 0xFFFFULL;

			const float clampedDepth = std::clamp(depth, 0.0f, 1000.0f);
			const uint64_t d = static_cast<uint64_t>((clampedDepth / 1000.0f) * 65535.0f) & 0xFFFFULL;

			return (l << 60) | (s << 48) | (m << 32) | (g << 16) | d;
		}

		bool operator<(const DrawItem& other) const noexcept
		{
			return sortKey < other.sortKey;
		}
	};

	struct BatchedDrawCall
	{
		uint32_t shaderVariantId = 0;
		uint32_t materialId = 0;
		uint32_t meshId = 0;
		uint32_t indexCount = 0;
		uint32_t startIndexLocation = 0;
		int32_t baseVertexLocation = 0;
		uint32_t instanceCount = 0;
		uint32_t firstInstanceIndex = 0;
	};

	inline std::vector<BatchedDrawCall> BatchDrawItems(
		std::span<const DrawItem> sortedItems,
		std::vector<InstanceData>& outInstances)
	{
		std::vector<BatchedDrawCall> batches;
		if (sortedItems.empty())
		{
			return batches;
		}

		outInstances.clear();
		outInstances.reserve(sortedItems.size());

		BatchedDrawCall currentBatch{};
		currentBatch.shaderVariantId = sortedItems[0].shaderVariantId;
		currentBatch.materialId = sortedItems[0].materialId;
		currentBatch.meshId = sortedItems[0].meshId;
		currentBatch.indexCount = sortedItems[0].indexCount;
		currentBatch.startIndexLocation = sortedItems[0].startIndexLocation;
		currentBatch.baseVertexLocation = sortedItems[0].baseVertexLocation;
		currentBatch.firstInstanceIndex = 0;
		currentBatch.instanceCount = 0;

		for (const auto& item : sortedItems)
		{
			if (item.shaderVariantId == currentBatch.shaderVariantId &&
				item.materialId == currentBatch.materialId &&
				item.meshId == currentBatch.meshId &&
				item.indexCount == currentBatch.indexCount &&
				item.startIndexLocation == currentBatch.startIndexLocation &&
				item.baseVertexLocation == currentBatch.baseVertexLocation)
			{
				currentBatch.instanceCount++;
				outInstances.push_back(item.instanceData);
			}
			else
			{
				batches.push_back(currentBatch);

				currentBatch.shaderVariantId = item.shaderVariantId;
				currentBatch.materialId = item.materialId;
				currentBatch.meshId = item.meshId;
				currentBatch.indexCount = item.indexCount;
				currentBatch.startIndexLocation = item.startIndexLocation;
				currentBatch.baseVertexLocation = item.baseVertexLocation;
				currentBatch.firstInstanceIndex = static_cast<uint32_t>(outInstances.size());
				currentBatch.instanceCount = 1;
				outInstances.push_back(item.instanceData);
			}
		}

		if (currentBatch.instanceCount > 0)
		{
			batches.push_back(currentBatch);
		}

		return batches;
	}
}
