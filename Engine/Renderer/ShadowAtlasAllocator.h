#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include <cstdint>
#include <vector>

namespace Core::Renderer
{
	struct AtlasSlot
	{
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t size = 0;
		bool isOccupied = false;
		bool isSplit = false;
		uint32_t lightIndex = 0xFFFFFFFF;
	};

	class DF_API ShadowAtlasAllocator
	{
	public:
		ShadowAtlasAllocator() = default;
		~ShadowAtlasAllocator() = default;

		void initialize(uint32_t atlasWidth = 4096, uint32_t atlasHeight = 4096);
		void reset();

		bool allocate(uint32_t lightIndex, uint32_t requestedSize, uint32_t& outX, uint32_t& outY, uint32_t& outSize, Math::Vector4& outSubRectUV);

		[[nodiscard]] uint32_t getAtlasWidth() const noexcept { return m_atlasWidth; }
		[[nodiscard]] uint32_t getAtlasHeight() const noexcept { return m_atlasHeight; }
		[[nodiscard]] uint32_t getReservedCornerSize() const noexcept { return m_reservedCornerSize; }

	private:
		uint32_t m_atlasWidth = 4096;
		uint32_t m_atlasHeight = 4096;
		uint32_t m_reservedCornerSize = 2048; // Directional CSM quadrant (0, 0, 2048, 2048)

		std::vector<AtlasSlot> m_slots;

		bool allocateNode(size_t nodeIndex, uint32_t requestedSize, uint32_t lightIndex, uint32_t& outX, uint32_t& outY, uint32_t& outSize);
	};
}
