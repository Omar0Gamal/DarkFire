#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include "Engine/RHI/IRHI.h"
#include <cstdint>
#include <memory>
#include <string>

namespace Core::RHI
{
	struct DF_API UploadAllocation
	{
		std::shared_ptr<IGpuBuffer> buffer;
		void* cpuData = nullptr;
		size_t byteOffset = 0;
		size_t byteSize = 0;
		size_t alignedSize = 0;
		uint32_t firstConstant16 = 0; // offset / 16, must be multiple of 16
		uint32_t numConstants16 = 0;  // alignedSize / 16, must be multiple of 16

		[[nodiscard]] bool isValid() const noexcept { return buffer != nullptr; }
	};

	class DF_API UploadRing
	{
	public:
		static constexpr size_t DEFAULT_CAPACITY = 16 * 1024 * 1024; // 16 MB
		static constexpr size_t ALIGNMENT = 256;                    // 256-byte alignment

		UploadRing() = default;
		~UploadRing() = default;

		Result<void, std::string> initialize(IRHIDevice& device, size_t capacity = DEFAULT_CAPACITY);
		void shutdown();

		UploadAllocation allocate(size_t size, const void* initialData = nullptr);
		bool write(const UploadAllocation& alloc, const void* data, size_t size);
		void reset();

		[[nodiscard]] size_t getCapacity() const noexcept { return m_capacity; }
		[[nodiscard]] size_t getHead() const noexcept { return m_head; }
		[[nodiscard]] size_t getAllocatedBytes() const noexcept { return m_allocatedBytes; }
		[[nodiscard]] uint32_t getWrapCount() const noexcept { return m_wrapCount; }
		[[nodiscard]] std::shared_ptr<IGpuBuffer> getBuffer() const noexcept { return m_buffer; }

	private:
		std::shared_ptr<IGpuBuffer> m_buffer;
		std::shared_ptr<IDeviceContext> m_context;
		size_t m_capacity = 0;
		size_t m_head = 0;
		size_t m_allocatedBytes = 0;
		uint32_t m_wrapCount = 0;
		bool m_needsDiscard = true;
	};
}
