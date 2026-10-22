#include "Engine/Core/VFS/VirtualFileSystem.h"
#include "Engine/Core/Log/Log.h"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace Core
{
#pragma pack(push, 1)
	struct PakHeaderOnDisk
	{
		char magic[4];          // "DFPK"
		uint32_t version;       // 1
		uint32_t entryCount;    // Number of entries
		uint64_t tableOffset;   // Offset to entry table
	};

	struct PakEntryOnDisk
	{
		uint64_t pathHash;
		uint64_t dataOffset;
		uint64_t dataSize;
		uint32_t pathLength;
	};
#pragma pack(pop)

	static constexpr uint64_t FNV1A_OFFSET = 14695981039346656037ULL;
	static constexpr uint64_t FNV1A_PRIME = 1099511628211ULL;

	uint64_t VirtualFileSystem::HashPath(std::string_view path) noexcept
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

	VirtualFileSystem::VirtualFileSystem() = default;

	std::string VirtualFileSystem::NormalizeVirtualPath(std::string_view path)
	{
		std::string result(path);
		std::replace(result.begin(), result.end(), '\\', '/');
		if (!result.empty() && result.front() != '/')
		{
			result.insert(result.begin(), '/');
		}
		return result;
	}

	void VirtualFileSystem::mount(std::string_view virtualPrefix, const std::filesystem::path& physicalPath)
	{
		std::unique_lock lock(m_mutex);
		std::string prefix = NormalizeVirtualPath(virtualPrefix);
		if (!prefix.empty() && prefix.back() != '/')
		{
			prefix += '/';
		}
		std::error_code ec;
		std::filesystem::path abs = std::filesystem::absolute(physicalPath.empty() ? "." : physicalPath, ec);
		m_mounts[prefix] = abs;
	}

	void VirtualFileSystem::unmount(std::string_view virtualPrefix)
	{
		std::unique_lock lock(m_mutex);
		std::string prefix = NormalizeVirtualPath(virtualPrefix);
		if (!prefix.empty() && prefix.back() != '/')
		{
			prefix += '/';
		}
		m_mounts.erase(prefix);
	}

	bool VirtualFileSystem::mountPak(std::string_view virtualPrefix, const std::filesystem::path& pakPath)
	{
		std::unique_lock lock(m_mutex);

		std::ifstream file(pakPath, std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		PakHeaderOnDisk header{};
		if (!file.read(reinterpret_cast<char*>(&header), sizeof(header)))
		{
			return false;
		}

		if (std::memcmp(header.magic, "DFPK", 4) != 0 || header.version != 1)
		{
			return false;
		}

		file.seekg(header.tableOffset, std::ios::beg);
		if (!file.good())
		{
			return false;
		}

		PakArchive archive;
		std::error_code ec;
		archive.pakFilePath = std::filesystem::absolute(pakPath, ec);
		archive.mountPrefix = NormalizeVirtualPath(virtualPrefix);
		if (!archive.mountPrefix.empty() && archive.mountPrefix.back() != '/')
		{
			archive.mountPrefix += '/';
		}

		for (uint32_t i = 0; i < header.entryCount; ++i)
		{
			PakEntryOnDisk diskEntry{};
			if (!file.read(reinterpret_cast<char*>(&diskEntry), sizeof(diskEntry)))
			{
				return false;
			}

			std::string vpath(diskEntry.pathLength, '\0');
			if (!file.read(vpath.data(), diskEntry.pathLength))
			{
				return false;
			}

			PakEntry entry;
			entry.pathHash = diskEntry.pathHash;
			entry.offset = diskEntry.dataOffset;
			entry.size = diskEntry.dataSize;
			entry.virtualPath = vpath;

			archive.entries[entry.pathHash] = entry;

			// Also index with prefixed virtual path if prefix is set
			if (!archive.mountPrefix.empty() && archive.mountPrefix != "/")
			{
				std::string prefixed = archive.mountPrefix;
				if (!vpath.empty() && vpath.front() == '/')
				{
					prefixed += vpath.substr(1);
				}
				else
				{
					prefixed += vpath;
				}
				uint64_t prefixedHash = HashPath(NormalizeVirtualPath(prefixed));
				archive.entries[prefixedHash] = entry;
			}
		}

		m_pakArchives.push_back(std::move(archive));
		return true;
	}

	void VirtualFileSystem::unmountPak(const std::filesystem::path& pakPath)
	{
		std::unique_lock lock(m_mutex);
		auto absPath = std::filesystem::absolute(pakPath);
		m_pakArchives.erase(
			std::remove_if(m_pakArchives.begin(), m_pakArchives.end(),
				[&](const PakArchive& a) { return a.pakFilePath == absPath; }),
			m_pakArchives.end());
	}

	Result<std::filesystem::path> VirtualFileSystem::resolve(std::string_view virtualPath) const
	{
		std::shared_lock lock(m_mutex);
		std::string norm = NormalizeVirtualPath(virtualPath);

		// Find best matching mount point (longest prefix match)
		const std::string* bestPrefix = nullptr;
		const std::filesystem::path* bestTarget = nullptr;

		for (const auto& [prefix, target] : m_mounts)
		{
			if (norm.rfind(prefix, 0) == 0) // starts_with
			{
				if (!bestPrefix || prefix.length() > bestPrefix->length())
				{
					bestPrefix = &prefix;
					bestTarget = &target;
				}
			}
		}

		if (!bestPrefix)
		{
			return Result<std::filesystem::path>::Err("No mount point found for virtual path: " + norm);
		}

		std::string relativePath = norm.substr(bestPrefix->length());
		std::filesystem::path resolved = *bestTarget / relativePath;
		return Result<std::filesystem::path>::Ok(resolved);
	}

	bool VirtualFileSystem::exists(std::string_view virtualPath) const
	{
		std::shared_lock lock(m_mutex);
		std::string norm = NormalizeVirtualPath(virtualPath);
		uint64_t hash = HashPath(norm);

		for (const auto& archive : m_pakArchives)
		{
			if (archive.entries.find(hash) != archive.entries.end())
			{
				return true;
			}
		}

		// Fallback to disk
		for (const auto& [prefix, target] : m_mounts)
		{
			if (norm.rfind(prefix, 0) == 0)
			{
				std::string relativePath = norm.substr(prefix.length());
				std::filesystem::path resolved = target / relativePath;
				if (std::filesystem::exists(resolved))
				{
					return true;
				}
			}
		}

		return false;
	}

	Result<std::vector<uint8_t>> VirtualFileSystem::readBinaryFile(std::string_view virtualPath) const
	{
		std::shared_lock lock(m_mutex);
		std::string norm = NormalizeVirtualPath(virtualPath);
		uint64_t hash = HashPath(norm);

		// 1. Check mounted Pak archives first
		for (const auto& archive : m_pakArchives)
		{
			auto it = archive.entries.find(hash);
			if (it != archive.entries.end())
			{
				std::ifstream file(archive.pakFilePath, std::ios::binary);
				if (!file.is_open())
				{
					return Result<std::vector<uint8_t>>::Err("Failed to open pak file: " + archive.pakFilePath.string());
				}

				file.seekg(it->second.offset, std::ios::beg);
				if (!file.good())
				{
					return Result<std::vector<uint8_t>>::Err("Failed to seek to pak entry offset: " + it->second.virtualPath);
				}

				std::vector<uint8_t> buffer(static_cast<size_t>(it->second.size));
				if (!file.read(reinterpret_cast<char*>(buffer.data()), it->second.size))
				{
					return Result<std::vector<uint8_t>>::Err("Failed to read pak entry data: " + it->second.virtualPath);
				}

				return Result<std::vector<uint8_t>>::Ok(std::move(buffer));
			}
		}

		// 2. Check disk mounts
		const std::string* bestPrefix = nullptr;
		const std::filesystem::path* bestTarget = nullptr;

		for (const auto& [prefix, target] : m_mounts)
		{
			if (norm.rfind(prefix, 0) == 0)
			{
				if (!bestPrefix || prefix.length() > bestPrefix->length())
				{
					bestPrefix = &prefix;
					bestTarget = &target;
				}
			}
		}

		if (!bestPrefix)
		{
			return Result<std::vector<uint8_t>>::Err("No mount point or pak entry found for virtual path: " + norm);
		}

		std::string relativePath = norm.substr(bestPrefix->length());
		std::filesystem::path resolved = *bestTarget / relativePath;

		std::ifstream file(resolved, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			return Result<std::vector<uint8_t>>::Err("Failed to open file: " + resolved.string());
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		{
			return Result<std::vector<uint8_t>>::Err("Failed to read file: " + resolved.string());
		}

		return Result<std::vector<uint8_t>>::Ok(std::move(buffer));
	}

	Result<std::string> VirtualFileSystem::readTextFile(std::string_view virtualPath) const
	{
		auto binaryRes = readBinaryFile(virtualPath);
		if (binaryRes.isErr())
		{
			return Result<std::string>::Err(binaryRes.error());
		}

		const auto& bytes = binaryRes.value();
		return Result<std::string>::Ok(std::string(bytes.begin(), bytes.end()));
	}

	bool VirtualFileSystem::CreatePakArchive(
		const std::filesystem::path& pakPath,
		const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files)
	{
		std::filesystem::create_directories(pakPath.parent_path());
		std::ofstream outFile(pakPath, std::ios::binary | std::ios::trunc);
		if (!outFile.is_open())
		{
			return false;
		}

		PakHeaderOnDisk header{};
		header.magic[0] = 'D';
		header.magic[1] = 'F';
		header.magic[2] = 'P';
		header.magic[3] = 'K';
		header.version = 1;
		header.entryCount = static_cast<uint32_t>(files.size());
		header.tableOffset = 0; // Will update after writing payload

		// Reserve header space
		outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

		struct EntryInfo
		{
			uint64_t hash;
			uint64_t offset;
			uint64_t size;
			std::string path;
		};

		std::vector<EntryInfo> entries;
		entries.reserve(files.size());

		// Write payload blobs
		for (const auto& [vpath, data] : files)
		{
			std::string norm = NormalizeVirtualPath(vpath);
			uint64_t hash = HashPath(norm);
			uint64_t offset = static_cast<uint64_t>(outFile.tellp());
			uint64_t size = static_cast<uint64_t>(data.size());

			if (!data.empty())
			{
				outFile.write(reinterpret_cast<const char*>(data.data()), size);
			}

			entries.push_back({ hash, offset, size, norm });
		}

		// Write entry table
		header.tableOffset = static_cast<uint64_t>(outFile.tellp());
		for (const auto& entry : entries)
		{
			PakEntryOnDisk diskEntry{};
			diskEntry.pathHash = entry.hash;
			diskEntry.dataOffset = entry.offset;
			diskEntry.dataSize = entry.size;
			diskEntry.pathLength = static_cast<uint32_t>(entry.path.size());

			outFile.write(reinterpret_cast<const char*>(&diskEntry), sizeof(diskEntry));
			outFile.write(entry.path.data(), diskEntry.pathLength);
		}

		// Rewrite header with valid tableOffset
		outFile.seekp(0, std::ios::beg);
		outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
		outFile.close();

		return true;
	}
}
