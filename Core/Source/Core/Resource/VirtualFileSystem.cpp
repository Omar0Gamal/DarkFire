#include "VirtualFileSystem.h"
#include "Core/Log/Log.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cwctype>

namespace Core
{
	// =========================================================================
	// PakArchive Implementation
	// =========================================================================

	PakArchive::PakArchive(const std::wstring& pakFilePath)
		: m_pakFilePath(pakFilePath)
	{
		std::ifstream file(pakFilePath, std::ios::binary);
		if (!file.is_open())
		{
			darklog.warn(L"PakArchive: Failed to open pak file: " + pakFilePath);
			return;
		}

		PakHeader header = {};
		file.read(reinterpret_cast<char*>(&header), sizeof(PakHeader));
		if (!file || header.magic[0] != 'D' || header.magic[1] != 'F' || header.magic[2] != 'P' || header.magic[3] != 'K')
		{
			darklog.warn(L"PakArchive: Invalid magic signature in " + pakFilePath);
			return;
		}

		if (header.version != 1)
		{
			darklog.warn(L"PakArchive: Unsupported pak version in " + pakFilePath);
			return;
		}

		file.seekg(header.tocOffset, std::ios::beg);
		if (!file)
		{
			darklog.warn(L"PakArchive: Failed to seek to TOC in " + pakFilePath);
			return;
		}

		for (uint32_t i = 0; i < header.fileCount; ++i)
		{
			PakFileEntryHeader entryHdr = {};
			file.read(reinterpret_cast<char*>(&entryHdr), sizeof(PakFileEntryHeader));
			if (!file)
				break;

			std::string rawPath;
			rawPath.resize(entryHdr.pathLength);
			file.read(&rawPath[0], entryHdr.pathLength);
			if (!file)
				break;

			PakEntry entry;
			entry.virtualPath = VirtualFileSystem::normalizePathA(rawPath);
			entry.uncompressedSize = entryHdr.uncompressedSize;
			entry.compressedSize = entryHdr.compressedSize;
			entry.dataOffset = entryHdr.dataOffset;
			entry.flags = entryHdr.flags;

			m_entries.emplace(entry.virtualPath, entry);
		}

		m_valid = true;
		darklog.info(L"PakArchive: Mounted '" + pakFilePath + L"' (" + std::to_wstring(m_entries.size()) + L" files indexed)");
	}

	PakArchive::~PakArchive()
	{
	}

	bool PakArchive::hasFile(const std::string& normalizedPath) const
	{
		return m_entries.find(normalizedPath) != m_entries.end();
	}

	bool PakArchive::readFile(const std::string& normalizedPath, std::vector<uint8_t>& outBuffer)
	{
		auto it = m_entries.find(normalizedPath);
		if (it == m_entries.end())
			return false;

		const PakEntry& entry = it->second;
		std::ifstream file(m_pakFilePath, std::ios::binary);
		if (!file.is_open())
			return false;

		file.seekg(entry.dataOffset, std::ios::beg);
		if (!file)
			return false;

		outBuffer.resize(static_cast<size_t>(entry.compressedSize));
		file.read(reinterpret_cast<char*>(outBuffer.data()), entry.compressedSize);
		return file.good();
	}

	// =========================================================================
	// VirtualFileSystem Implementation
	// =========================================================================

	VirtualFileSystem& VirtualFileSystem::get()
	{
		static VirtualFileSystem instance;
		return instance;
	}

	VirtualFileSystem::VirtualFileSystem()
	{
		// Default search paths for local development
		mountDirectory(L"");
		mountDirectory(L"Assets/");
		mountDirectory(L"Games/KartGame/Assets/");
		mountDirectory(L"SandBox/Assets/");
	}

	std::string VirtualFileSystem::normalizePathA(const std::string& path)
	{
		std::string result = path;
		std::replace(result.begin(), result.end(), '\\', '/');
		for (auto& c : result)
		{
			c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		}
		// Strip leading "./" or "/"
		while (result.rfind("./", 0) == 0)
		{
			result = result.substr(2);
		}
		while (!result.empty() && result.front() == '/')
		{
			result = result.substr(1);
		}
		return result;
	}

	std::wstring VirtualFileSystem::normalizePathW(const std::wstring& path)
	{
		std::wstring result = path;
		std::replace(result.begin(), result.end(), L'\\', L'/');
		for (auto& c : result)
		{
			c = static_cast<wchar_t>(towlower(c));
		}
		while (result.rfind(L"./", 0) == 0)
		{
			result = result.substr(2);
		}
		while (!result.empty() && result.front() == L'/')
		{
			result = result.substr(1);
		}
		return result;
	}

	bool VirtualFileSystem::mountDirectory(const std::wstring& directoryPath, const std::wstring& mountPrefix)
	{
		std::wstring norm = directoryPath;
		std::replace(norm.begin(), norm.end(), L'\\', L'/');
		if (!norm.empty() && norm.back() != L'/')
		{
			norm += L'/';
		}

		if (std::find(m_looseMounts.begin(), m_looseMounts.end(), norm) == m_looseMounts.end())
		{
			m_looseMounts.push_back(norm);
		}
		return true;
	}

	bool VirtualFileSystem::mountPak(const std::wstring& pakFilePath)
	{
		auto pak = std::make_shared<PakArchive>(pakFilePath);
		if (pak->isValid())
		{
			m_pakMounts.push_back(pak);
			return true;
		}
		return false;
	}

	bool VirtualFileSystem::mount(const std::wstring& path, const std::wstring& mountPrefix)
	{
		std::filesystem::path p(path);
		if (p.extension() == L".pak")
		{
			return mountPak(path);
		}
		return mountDirectory(path, mountPrefix);
	}

	void VirtualFileSystem::unmountAll()
	{
		m_looseMounts.clear();
		m_pakMounts.clear();
	}

	bool VirtualFileSystem::resolvePhysicalPath(const std::wstring& virtualPath, std::wstring& outPhysicalPath) const
	{
		std::wstring norm = virtualPath;
		std::replace(norm.begin(), norm.end(), L'\\', L'/');

		if (std::filesystem::exists(norm))
		{
			outPhysicalPath = norm;
			return true;
		}

		for (const auto& mount : m_looseMounts)
		{
			std::wstring candidate = mount + norm;
			if (std::filesystem::exists(candidate))
			{
				outPhysicalPath = candidate;
				return true;
			}
		}

		return false;
	}

	bool VirtualFileSystem::fileExists(const std::wstring& virtualPath) const
	{
		std::wstring dummy;
		if (resolvePhysicalPath(virtualPath, dummy))
			return true;

		std::string normA;
		normA.reserve(virtualPath.size());
		for (wchar_t wc : virtualPath)
		{
			normA.push_back(static_cast<char>(wc));
		}
		normA = normalizePathA(normA);

		for (const auto& pak : m_pakMounts)
		{
			if (pak->hasFile(normA))
				return true;
		}

		return false;
	}

	bool VirtualFileSystem::readFile(const std::wstring& virtualPath, std::vector<uint8_t>& outBuffer) const
	{
		std::string normA;
		normA.reserve(virtualPath.size());
		for (wchar_t wc : virtualPath)
		{
			normA.push_back(static_cast<char>(wc));
		}
		normA = normalizePathA(normA);

		auto readFromLoose = [&]() -> bool
		{
			std::wstring physicalPath;
			if (resolvePhysicalPath(virtualPath, physicalPath))
			{
				std::ifstream file(physicalPath, std::ios::binary | std::ios::ate);
				if (file.is_open())
				{
					std::streamsize size = file.tellg();
					file.seekg(0, std::ios::beg);
					outBuffer.resize(static_cast<size_t>(size));
					return file.read(reinterpret_cast<char*>(outBuffer.data()), size).good();
				}
			}
			return false;
		};

		auto readFromPaks = [&]() -> bool
		{
			for (const auto& pak : m_pakMounts)
			{
				if (pak->hasFile(normA))
				{
					return pak->readFile(normA, outBuffer);
				}
			}
			return false;
		};

		if (m_preferLoose)
		{
			if (readFromLoose())
				return true;
			return readFromPaks();
		}
		else
		{
			if (readFromPaks())
				return true;
			return readFromLoose();
		}
	}
}
