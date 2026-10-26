#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
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

	// 64-bit Content-Hash Incremental Cook Cache (Milestone 8.2)
	struct CookCache
	{
		std::unordered_map<std::string, uint64_t> hashes;

		inline static uint64_t ComputeFileHash(const std::filesystem::path& filePath)
		{
			std::error_code ec;
			auto sz = std::filesystem::file_size(filePath, ec);
			if (ec) return 0;
			auto lwt = std::filesystem::last_write_time(filePath, ec).time_since_epoch().count();
			uint64_t h = FNV1A_OFFSET;
			h ^= static_cast<uint64_t>(sz);
			h *= FNV1A_PRIME;
			h ^= static_cast<uint64_t>(lwt);
			h *= FNV1A_PRIME;
			return h;
		}

		inline bool load(const std::filesystem::path& cachePath)
		{
			hashes.clear();
			if (!std::filesystem::exists(cachePath)) return false;
			std::ifstream f(cachePath);
			if (!f.is_open()) return false;
			std::string key;
			uint64_t h = 0;
			while (f >> key >> h)
			{
				hashes[key] = h;
			}
			return true;
		}

		inline bool save(const std::filesystem::path& cachePath) const
		{
			std::filesystem::create_directories(cachePath.parent_path());
			std::ofstream f(cachePath);
			if (!f.is_open()) return false;
			for (const auto& [k, h] : hashes)
			{
				f << k << " " << h << "\n";
			}
			return true;
		}

		inline bool isUpToDate(const std::filesystem::path& sourcePath, const std::string& virtualKey) const
		{
			auto it = hashes.find(virtualKey);
			if (it == hashes.end()) return false;
			return it->second == ComputeFileHash(sourcePath);
		}

		inline void update(const std::filesystem::path& sourcePath, const std::string& virtualKey)
		{
			hashes[virtualKey] = ComputeFileHash(sourcePath);
		}
	};

	std::vector<uint8_t> ReadDiskFile(const std::filesystem::path& path);

	bool CookShaders(const std::filesystem::path& shadersInDir, const std::filesystem::path& shadersOutDir, CookCache* cache = nullptr);
	bool CookTextures(const std::filesystem::path& inDir, const std::filesystem::path& outDir, CookCache* cache = nullptr);
	bool CookLightingIBL(const std::filesystem::path& outBakedDir, BakeQuality quality = BakeQuality::Production);
	bool ConvolveProbeHDR(const std::filesystem::path& inHdrPath, const std::filesystem::path& outDdsPath, BakeQuality quality = BakeQuality::Production);
	bool CookMeshes(const std::filesystem::path& inDir, const std::filesystem::path& outDir, CookCache* cache = nullptr);
	void WriteManifest(const std::filesystem::path& manifestPath, const std::vector<CookedAsset>& assets);
}
