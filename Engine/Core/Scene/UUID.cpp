#include "UUID.h"
#include "Engine/Core/Random.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace Core
{
	const UUID UUID::Null{ 0 };

	namespace
	{
		// Thread-safe random device + SplitMix64 generator for UUID initialization
		static uint64_t GenerateSecureRandom64()
		{
			static thread_local std::random_device rd;
			static thread_local std::mt19937_64 gen(rd());
			static thread_local std::uniform_int_distribution<uint64_t> dis(1, UINT64_MAX);
			return dis(gen);
		}
	}

	UUID::UUID()
		: m_uuid(GenerateSecureRandom64())
	{
		if (m_uuid == 0)
		{
			m_uuid = 1;
		}
	}

	UUID::UUID(uint64_t uuid)
		: m_uuid(uuid)
	{
	}

	std::string UUID::toString() const
	{
		std::stringstream ss;
		ss << std::hex << std::setfill('0') << std::setw(16) << m_uuid;
		return ss.str();
	}
}
