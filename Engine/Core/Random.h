#pragma once

#include "Engine/Core/Defines.h"
#include <cstdint>

namespace Core
{
	class DF_API Random
	{
	public:
		Random();
		explicit Random(uint64_t initialSeed);

		void seed(uint64_t s);
		uint64_t nextU64();
		uint32_t nextU32();
		float nextFloat(); // [0.0f, 1.0f)
		float nextFloatRange(float min, float max);
		int32_t nextIntRange(int32_t min, int32_t max);

	private:
		static uint64_t rotl(uint64_t x, int k) noexcept
		{
			return (x << k) | (x >> (64 - k));
		}

		uint64_t m_s[4]{};
	};
}
