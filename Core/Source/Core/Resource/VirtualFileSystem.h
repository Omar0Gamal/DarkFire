#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace Core
{
	#pragma pack(push, 1)
	struct PakHeader
	{
		char magic[4];          // "DFPK"
		uint32_t version;       // 1
		uint32_t fileCount;
		uint64_t tocOffset;
	};

	struct PakFileEntryHeader
	{
		uint16_t pathLength;
		uint64_t uncompressedSize;
		uint64_t compressedSize;
		uint64_t dataOffset;
		uint32_t flags;
	};
	#pragma pack(pop)

	struct PakEntry
	{
		std::string virtualPath; // Normalized lower-case forward-slash path e.g. "assets/textures/car.png"
		uint64_t uncompressedSize = 0;
		uint64_t compressedSize = 0;
		uint64_t dataOffset = 0;
		uint32_t flags = 0;
	};

	class PakArchive
	{
	public:
		PakArchive(const std::wstring& pakFilePath);
		~PakArchive();

		bool isValid() const { return m_valid; }
		const std::wstring& getFilePath() const { return m_pakFilePath; }
		bool hasFile(const std::string& normalizedPath) const;
		bool readFile(const std::string& normalizedPath, std::vector<uint8_t>& outBuffer);
		const std::unordered_map<std::string, PakEntry>& getEntries() const { return m_entries; }

	private:
		std::wstring m_pakFilePath;
		bool m_valid = false;
		std::unordered_map<std::string, PakEntry> m_entries;
	};

	class VirtualFileSystem
	{
	public:
		static VirtualFileSystem& get();

		// Mount loose directory (e.g. L"Assets/", L"Games/KartGame/Assets/")
		bool mountDirectory(const std::wstring& directoryPath, const std::wstring& mountPrefix = L"");

		// Mount .pak package file (e.g. L"GameData.pak")
		bool mountPak(const std::wstring& pakFilePath);

		// Mount path automatically detecting whether it is a directory or .pak file
		bool mount(const std::wstring& path, const std::wstring& mountPrefix = L"");

		// Unmount all mounted points
		void unmountAll();

		// Check if file exists across all mounted providers (loose folders and pak archives)
		bool fileExists(const std::wstring& virtualPath) const;

		// Read entire file content into binary memory buffer
		bool readFile(const std::wstring& virtualPath, std::vector<uint8_t>& outBuffer) const;

		// Resolve to physical disk path if available as a loose file on disk
		bool resolvePhysicalPath(const std::wstring& virtualPath, std::wstring& outPhysicalPath) const;

		// Preference control: when true, check loose directory mounts first (development/hot-reload mode)
		void setPreferLooseFiles(bool prefer) { m_preferLoose = prefer; }
		bool getPreferLooseFiles() const { return m_preferLoose; }

		// Path normalization helpers
		static std::string normalizePathA(const std::string& path);
		static std::wstring normalizePathW(const std::wstring& path);

	private:
		VirtualFileSystem();
		~VirtualFileSystem() = default;
		VirtualFileSystem(const VirtualFileSystem&) = delete;
		VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

	private:
		bool m_preferLoose = true;
		std::vector<std::wstring> m_looseMounts;
		std::vector<std::shared_ptr<PakArchive>> m_pakMounts;
	};
}
