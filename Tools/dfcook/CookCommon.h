#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace dfcook
{
#pragma pack(push, 1)
	struct PakHeader
	{
		char magic[4];          // "DFPK"
		uint32_t version;       // 1
		uint32_t entryCount;    // Number of entries
		uint64_t tableOffset;   // Offset to entry table
	};

	struct PakDiskEntry
	{
		uint64_t pathHash;
		uint64_t dataOffset;
		uint64_t dataSize;
		uint32_t pathLength;
	};
#pragma pack(pop)

	static constexpr uint64_t FNV1A_OFFSET = 14695981039346656037ULL;
	static constexpr uint64_t FNV1A_PRIME = 1099511628211ULL;

	inline uint64_t HashPath(std::string_view path) noexcept
	{
		uint64_t hash = FNV1A_OFFSET;
		for (char c : path)
		{
			char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : (c == '\\' ? '/' : c);
			hash ^= static_cast<uint64_t>(static_cast<uint8_t>(lower));
			hash *= FNV1A_PRIME;
		}
		return hash;
	}

	inline std::string NormalizeVirtualPath(std::string_view path)
	{
		std::string result(path);
		std::replace(result.begin(), result.end(), '\\', '/');
		if (!result.empty() && result.front() != '/')
		{
			result.insert(result.begin(), '/');
		}
		return result;
	}

	struct CookedAsset
	{
		std::string virtualPath;
		uint64_t size = 0;
		uint64_t hash = 0;
	};

	enum class BakeQuality
	{
		Fast = 0,
		Balanced = 1,
		Production = 2
	};

	std::vector<uint8_t> ReadDiskFile(const std::filesystem::path& path);

	bool CookShaders(const std::filesystem::path& shadersInDir, const std::filesystem::path& shadersOutDir);
	bool CookTextures(const std::filesystem::path& inDir, const std::filesystem::path& outDir);
	bool CookLightingIBL(const std::filesystem::path& outBakedDir, BakeQuality quality = BakeQuality::Production);
	bool ConvolveProbeHDR(const std::filesystem::path& inHdrPath, const std::filesystem::path& outDdsPath, BakeQuality quality = BakeQuality::Production);
}
