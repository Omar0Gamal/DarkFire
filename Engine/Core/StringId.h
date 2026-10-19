#pragma once

#include "Engine/Core/Defines.h"
#include <cstdint>
#include <string_view>
#include <functional>

namespace Core
{
	class StringId
	{
	public:
		static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
		static constexpr uint64_t FNV_PRIME = 1099511628211ull;

		static constexpr uint64_t Hash(std::string_view str) noexcept
		{
			uint64_t hash = FNV_OFFSET_BASIS;
			for (char c : str)
			{
				hash ^= static_cast<uint64_t>(c);
				hash *= FNV_PRIME;
			}
			return hash;
		}

		constexpr StringId() noexcept : m_value(0) {}
		constexpr explicit StringId(uint64_t val) noexcept : m_value(val) {}
		constexpr explicit StringId(std::string_view str) noexcept : m_value(Hash(str)) {}

		constexpr uint64_t value() const noexcept { return m_value; }
		constexpr explicit operator uint64_t() const noexcept { return m_value; }

		constexpr bool operator==(const StringId& other) const noexcept { return m_value == other.m_value; }
		constexpr bool operator!=(const StringId& other) const noexcept { return m_value != other.m_value; }
		constexpr bool operator<(const StringId& other) const noexcept { return m_value < other.m_value; }

	private:
		uint64_t m_value;
	};
}

constexpr Core::StringId operator""_sid(const char* s, size_t count) noexcept
{
	return Core::StringId(std::string_view(s, count));
}

namespace std
{
	template<>
	struct hash<Core::StringId>
	{
		size_t operator()(const Core::StringId& id) const noexcept
		{
			return static_cast<size_t>(id.value());
		}
	};
}
