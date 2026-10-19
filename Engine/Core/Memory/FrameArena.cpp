#include "Engine/Core/Memory/FrameArena.h"
#include <cstdlib>
#include <algorithm>

namespace Core
{
	FrameArena::FrameArena(size_t capacity)
		: m_capacity(capacity)
		, m_resource(this)
	{
		m_buffer = static_cast<uint8_t*>(_aligned_malloc(m_capacity, 64));
		DF_ASSERT(m_buffer != nullptr, "Failed to allocate FrameArena buffer!");
	}

	FrameArena::~FrameArena()
	{
		if (m_buffer)
		{
			_aligned_free(m_buffer);
			m_buffer = nullptr;
		}
	}

	void* FrameArena::allocate(size_t bytes, size_t alignment)
	{
		if (bytes == 0) return nullptr;

		// Calculate aligned offset
		uintptr_t currentPtr = reinterpret_cast<uintptr_t>(m_buffer + m_used);
		uintptr_t alignedPtr = (currentPtr + (alignment - 1)) & ~(alignment - 1);
		size_t padding = alignedPtr - currentPtr;

		size_t totalBytes = bytes + padding;
		if (m_used + totalBytes > m_capacity)
		{
			DF_ASSERT(false, "FrameArena out of memory! Capacity exceeded.");
			return nullptr;
		}

		m_used += totalBytes;
		m_highWaterMark = std::max(m_highWaterMark, m_used);

		return reinterpret_cast<void*>(alignedPtr);
	}

	void FrameArena::reset()
	{
		m_used = 0;
	}
}
