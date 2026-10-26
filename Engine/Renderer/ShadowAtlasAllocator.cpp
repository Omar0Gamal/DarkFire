#include "ShadowAtlasAllocator.h"
#include <algorithm>

namespace Core::Renderer
{
	void ShadowAtlasAllocator::initialize(uint32_t atlasWidth, uint32_t atlasHeight)
	{
		m_atlasWidth = atlasWidth;
		m_atlasHeight = atlasHeight;
		m_reservedCornerSize = std::min(atlasWidth, atlasHeight) / 2;

		reset();
	}

	void ShadowAtlasAllocator::reset()
	{
		m_slots.clear();

		// Divide atlas into 4 quadrants of size (m_reservedCornerSize x m_reservedCornerSize).
		// Quadrant 0 (0, 0) is reserved for Directional 4-Cascade CSM.
		// Quadrants 1, 2, 3 are dynamic sub-allocatable roots.
		uint32_t halfW = m_reservedCornerSize;
		uint32_t halfH = m_reservedCornerSize;

		// Root for Quadrant 1 (Top-Right: halfW, 0)
		AtlasSlot q1{};
		q1.x = halfW;
		q1.y = 0;
		q1.size = halfW;
		m_slots.push_back(q1);

		// Root for Quadrant 2 (Bottom-Left: 0, halfH)
		AtlasSlot q2{};
		q2.x = 0;
		q2.y = halfH;
		q2.size = halfH;
		m_slots.push_back(q2);

		// Root for Quadrant 3 (Bottom-Right: halfW, halfH)
		AtlasSlot q3{};
		q3.x = halfW;
		q3.y = halfH;
		q3.size = halfW;
		m_slots.push_back(q3);
	}

	bool ShadowAtlasAllocator::allocateNode(size_t nodeIndex, uint32_t requestedSize, uint32_t lightIndex, uint32_t& outX, uint32_t& outY, uint32_t& outSize)
	{
		if (nodeIndex >= m_slots.size()) return false;

		auto& slot = m_slots[nodeIndex];
		if (slot.isOccupied) return false;

		if (slot.isSplit)
		{
			return false;
		}

		if (slot.size == requestedSize)
		{
			slot.isOccupied = true;
			slot.lightIndex = lightIndex;
			outX = slot.x;
			outY = slot.y;
			outSize = slot.size;
			return true;
		}

		if (slot.size > requestedSize)
		{
			// Split this slot into 4 children
			slot.isSplit = true;
			uint32_t half = slot.size / 2;

			AtlasSlot c0{ slot.x,        slot.y,        half, false, false, 0xFFFFFFFF };
			AtlasSlot c1{ slot.x + half, slot.y,        half, false, false, 0xFFFFFFFF };
			AtlasSlot c2{ slot.x,        slot.y + half, half, false, false, 0xFFFFFFFF };
			AtlasSlot c3{ slot.x + half, slot.y + half, half, false, false, 0xFFFFFFFF };

			size_t childStart = m_slots.size();
			m_slots.push_back(c0);
			m_slots.push_back(c1);
			m_slots.push_back(c2);
			m_slots.push_back(c3);

			// Allocate from first child
			return allocateNode(childStart, requestedSize, lightIndex, outX, outY, outSize);
		}

		return false;
	}

	bool ShadowAtlasAllocator::allocate(
		uint32_t lightIndex,
		uint32_t requestedSize,
		uint32_t& outX,
		uint32_t& outY,
		uint32_t& outSize,
		Math::Vector4& outSubRectUV)
	{
		// Clamp requestedSize to power of 2: 256, 512, 1024, or 2048
		uint32_t targetSize = 256;
		if (requestedSize >= 1536) targetSize = 2048;
		else if (requestedSize >= 768) targetSize = 1024;
		else if (requestedSize >= 384) targetSize = 512;
		else targetSize = 256;

		// Scan available leaves
		for (size_t i = 0; i < m_slots.size(); ++i)
		{
			if (!m_slots[i].isOccupied && !m_slots[i].isSplit && m_slots[i].size >= targetSize)
			{
				if (allocateNode(i, targetSize, lightIndex, outX, outY, outSize))
				{
					float invW = 1.0f / static_cast<float>(m_atlasWidth);
					float invH = 1.0f / static_cast<float>(m_atlasHeight);
					outSubRectUV = Math::Vector4(
						static_cast<float>(outX) * invW,
						static_cast<float>(outY) * invH,
						static_cast<float>(outSize) * invW,
						static_cast<float>(outSize) * invH
					);
					return true;
				}
			}
		}

		return false;
	}
}
