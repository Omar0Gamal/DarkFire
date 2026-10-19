#include "UploadRing.h"
#include "Engine/Core/Log/Log.h"
#include <cstring>

namespace Core::RHI
{
	Result<void, std::string> UploadRing::initialize(IRHIDevice& device, size_t capacity)
	{
		m_capacity = capacity;
		m_head = 0;
		m_allocatedBytes = 0;
		m_wrapCount = 0;
		m_needsDiscard = true;
		m_context = device.getImmediate();

		GpuBufferDesc desc{};
		desc.size = m_capacity;
		desc.usage = BufferUsage::Dynamic;
		desc.bindFlags = BufferBindFlags::ConstantBuffer;
		desc.structureByteStride = 0;

		auto bufferResult = device.createBuffer(desc, nullptr);
		if (!bufferResult)
		{
			return Result<void, std::string>::error("Failed to create UploadRing dynamic constant buffer: " + bufferResult.error());
		}

		m_buffer = *bufferResult;
		return Result<void, std::string>::ok();
	}

	void UploadRing::shutdown()
	{
		m_buffer.reset();
		m_context.reset();
		m_capacity = 0;
		m_head = 0;
		m_allocatedBytes = 0;
		m_wrapCount = 0;
		m_needsDiscard = true;
	}

	UploadAllocation UploadRing::allocate(size_t size, const void* initialData)
	{
		if (size == 0 || m_capacity == 0)
		{
			return {};
		}

		const size_t alignedSize = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
		if (alignedSize > m_capacity)
		{
			DF_LOG_ERROR("RHI", "UploadRing: Allocation size {} exceeds total capacity {}", alignedSize, m_capacity);
			return {};
		}

		if (m_head + alignedSize > m_capacity)
		{
			m_head = 0;
			m_wrapCount++;
			m_needsDiscard = true;
		}

		const size_t offset = m_head;
		m_head += alignedSize;
		m_allocatedBytes += alignedSize;

		UploadAllocation alloc{};
		alloc.buffer = m_buffer;
		alloc.byteOffset = offset;
		alloc.byteSize = size;
		alloc.alignedSize = alignedSize;
		alloc.firstConstant16 = static_cast<uint32_t>(offset / 16);
		alloc.numConstants16 = static_cast<uint32_t>(alignedSize / 16);

		if (initialData != nullptr && m_context && m_buffer)
		{
			write(alloc, initialData, size);
		}

		return alloc;
	}

	bool UploadRing::write(const UploadAllocation& alloc, const void* data, size_t size)
	{
		if (!alloc.isValid() || !m_context || !data || size == 0 || size > alloc.byteSize)
		{
			return false;
		}

		auto mapResult = m_context->map(alloc.buffer, m_needsDiscard);
		if (!mapResult)
		{
			DF_LOG_ERROR("RHI", "UploadRing: Failed to map dynamic constant buffer: {}", mapResult.error());
			return false;
		}

		m_needsDiscard = false;
		auto* dest = static_cast<uint8_t*>(*mapResult);
		std::memcpy(dest + alloc.byteOffset, data, size);
		m_context->unmap(alloc.buffer);
		return true;
	}

	void UploadRing::reset()
	{
		m_head = 0;
		m_needsDiscard = true;
	}
}
