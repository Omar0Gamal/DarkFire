#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>

namespace Core
{
	struct PakEntry
	{
		uint64_t pathHash = 0;
		uint64_t offset = 0;
		uint64_t size = 0;
		std::string virtualPath;
	};

	struct PakArchive
	{
		std::filesystem::path pakFilePath;
		std::string mountPrefix;
		std::unordered_map<uint64_t, PakEntry> entries;
	};

	class DF_API VirtualFileSystem
	{
	public:
		VirtualFileSystem();
		~VirtualFileSystem() = default;

		void mount(std::string_view virtualPrefix, const std::filesystem::path& physicalPath);
		void unmount(std::string_view virtualPrefix);

		bool mountPak(std::string_view virtualPrefix, const std::filesystem::path& pakPath);
		void unmountPak(const std::filesystem::path& pakPath);

		Result<std::filesystem::path> resolve(std::string_view virtualPath) const;
		bool exists(std::string_view virtualPath) const;

		Result<std::vector<uint8_t>> readBinaryFile(std::string_view virtualPath) const;
		Result<std::string> readTextFile(std::string_view virtualPath) const;

		static std::string NormalizeVirtualPath(std::string_view path);
		static uint64_t HashPath(std::string_view path) noexcept;

		// Pak Archive Generator
		static bool CreatePakArchive(
			const std::filesystem::path& pakPath,
			const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);

	private:
		mutable std::shared_mutex m_mutex;
		std::map<std::string, std::filesystem::path> m_mounts;
		std::vector<PakArchive> m_pakArchives;
	};
}
