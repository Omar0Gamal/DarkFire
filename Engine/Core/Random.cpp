#include "Engine/Core/Random.h"
#include <random>

namespace Core
{
	namespace
	{
		// SplitMix64 generator used to initialize Xoshiro256** state from a single 64-bit seed
		uint64_t SplitMix64(uint64_t& x) noexcept
		{
			uint64_t z = (x += 0x9e3779b97f4a7c15ull);
			z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
			z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
			return z ^ (z >> 31);
		}
	}

	Random::Random()
	{
		std::random_device rd;
		uint64_t s = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
		seed(s);
	}

	Random::Random(uint64_t initialSeed)
	{
		seed(initialSeed);
	}

	void Random::seed(uint64_t s)
	{
		uint64_t sm = s;
		m_s[0] = SplitMix64(sm);
		m_s[1] = SplitMix64(sm);
		m_s[2] = SplitMix64(sm);
		m_s[3] = SplitMix64(sm);
		if (m_s[0] == 0 && m_s[1] == 0 && m_s[2] == 0 && m_s[3] == 0)
		{
			m_s[0] = 0x9e3779b97f4a7c15ull;
		}
	}

	uint64_t Random::nextU64()
	{
		const uint64_t result = rotl(m_s[1] * 5, 7) * 9;
		const uint64_t t = m_s[1] << 17;

		m_s[2] ^= m_s[0];
		m_s[3] ^= m_s[1];
		m_s[1] ^= m_s[2];
		m_s[0] ^= m_s[3];

		m_s[2] ^= t;
		m_s[3] = rotl(m_s[3], 45);

		return result;
	}

	uint32_t Random::nextU32()
	{
		return static_cast<uint32_t>(nextU64() >> 32);
	}

	float Random::nextFloat()
	{
		return static_cast<float>((nextU64() >> 40)) * (1.0f / 16777216.0f);
	}

	float Random::nextFloatRange(float min, float max)
	{
		return min + (max - min) * nextFloat();
	}

	int32_t Random::nextIntRange(int32_t min, int32_t max)
	{
		if (min >= max) return min;
		uint32_t range = static_cast<uint32_t>(max - min + 1);
		return min + static_cast<int32_t>(nextU32() % range);
	}
}
