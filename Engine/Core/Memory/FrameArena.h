#pragma once

#include "Engine/Core/Defines.h"
#include <memory_resource>
#include <cstdint>
#include <cstddef>

namespace Core
{
	class DF_API FrameArena
	{
	public:
		static constexpr size_t DEFAULT_CAPACITY = 16 * 1024 * 1024; // 16 MB

		explicit FrameArena(size_t capacity = DEFAULT_CAPACITY);
		~FrameArena();

		FrameArena(const FrameArena&) = delete;
		FrameArena& operator=(const FrameArena&) = delete;

		void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));
		void reset();

		size_t getUsedBytes() const { return m_used; }
		size_t getHighWaterMark() const { return m_highWaterMark; }
		size_t getCapacity() const { return m_capacity; }

		std::pmr::memory_resource* resource() { return &m_resource; }

	private:
		class ArenaResource : public std::pmr::memory_resource
		{
		public:
			explicit ArenaResource(FrameArena* arena) : m_arena(arena) {}
		protected:
			void* do_allocate(size_t bytes, size_t alignment) override { return m_arena->allocate(bytes, alignment); }
			void do_deallocate(void*, size_t, size_t) override {}
			bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
		private:
			FrameArena* m_arena;
		};

		uint8_t* m_buffer = nullptr;
		size_t m_capacity = 0;
		size_t m_used = 0;
		size_t m_highWaterMark = 0;
		ArenaResource m_resource;
	};
}
