#pragma once

#include "Engine/Core/Defines.h"
#include <cstdint>
#include <functional>
#include <string>

namespace Core
{
	class DF_API UUID
	{
	public:
		UUID();
		explicit UUID(uint64_t uuid);
		UUID(const UUID&) = default;
		UUID& operator=(const UUID&) = default;

		uint64_t value() const noexcept { return m_uuid; }
		bool isValid() const noexcept { return m_uuid != 0; }
		std::string toString() const;

		operator uint64_t() const noexcept { return m_uuid; }
		bool operator==(const UUID& other) const noexcept { return m_uuid == other.m_uuid; }
		bool operator!=(const UUID& other) const noexcept { return m_uuid != other.m_uuid; }
		bool operator<(const UUID& other) const noexcept { return m_uuid < other.m_uuid; }

		static const UUID Null;

	private:
		uint64_t m_uuid = 0;
	};
}

namespace std
{
	template<>
	struct hash<Core::UUID>
	{
		size_t operator()(const Core::UUID& uuid) const noexcept
		{
			return std::hash<uint64_t>{}(uuid.value());
		}
	};
}
