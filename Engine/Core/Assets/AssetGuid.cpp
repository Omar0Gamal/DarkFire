#include "Engine/Core/Assets/AssetGuid.h"
#include <random>
#include <charconv>

namespace Core
{
	const AssetGuid AssetGuid::Empty{ 0 };

	namespace
	{
		static uint64_t GenerateGuid64()
		{
			static thread_local std::random_device rd;
			static thread_local std::mt19937_64 gen(rd());
			static thread_local std::uniform_int_distribution<uint64_t> dis(1, UINT64_MAX);
			return dis(gen);
		}
	}

	AssetGuid::AssetGuid()
		: m_value(GenerateGuid64())
	{
		if (m_value == 0)
		{
			m_value = 1;
		}
	}

	AssetGuid::AssetGuid(uint64_t value)
		: m_value(value)
	{
	}

	std::string AssetGuid::toString() const
	{
		return std::to_string(m_value);
	}

	AssetGuid AssetGuid::fromString(std::string_view str)
	{
		uint64_t val = 0;
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
		if (ec == std::errc())
		{
			return AssetGuid(val);
		}
		return AssetGuid::Empty;
	}
}
