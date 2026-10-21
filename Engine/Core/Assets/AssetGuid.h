#pragma once

#include "Engine/Core/Defines.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <functional>

namespace Core
{
	class DF_API AssetGuid
	{
	public:
		AssetGuid();
		explicit AssetGuid(uint64_t value);

		uint64_t value() const noexcept { return m_value; }
		bool isValid() const noexcept { return m_value != 0; }

		std::string toString() const;
		static AssetGuid fromString(std::string_view str);

		bool operator==(const AssetGuid& other) const noexcept { return m_value == other.m_value; }
		bool operator!=(const AssetGuid& other) const noexcept { return !(*this == other); }
		bool operator<(const AssetGuid& other) const noexcept { return m_value < other.m_value; }

		static const AssetGuid Empty;

	private:
		uint64_t m_value = 0;
	};
}

namespace std
{
	template<>
	struct hash<Core::AssetGuid>
	{
		size_t operator()(const Core::AssetGuid& guid) const noexcept
		{
			return std::hash<uint64_t>{}(guid.value());
		}
	};
}
