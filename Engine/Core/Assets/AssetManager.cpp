#include "Engine/Core/Assets/AssetManager.h"
#include "Engine/Core/Jobs/ThreadPool.h"
#include "Engine/Core/Log/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace Core
{
	AssetManager& AssetManager::Get()
	{
		static AssetManager instance;
		return instance;
	}

	void AssetManager::init(const std::filesystem::path& projectRoot)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_projectRoot = projectRoot;
		m_assets.clear();
		m_pathToGuid.clear();
		m_initialized = true;

		if (!m_projectRoot.empty() && std::filesystem::exists(m_projectRoot))
		{
			scanDirectory(m_projectRoot);
		}
	}

	void AssetManager::shutdown()
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_assets.clear();
		m_pathToGuid.clear();
		m_initialized = false;
	}

	std::string AssetManager::normalizePath(const std::filesystem::path& path) const
	{
		std::string s = path.generic_string();
		if (s.rfind("./", 0) == 0)
		{
			s = s.substr(2);
		}
		return s;
	}

	bool AssetManager::readMetaFile(const std::filesystem::path& metaPath, AssetGuid& outGuid, AssetType& outType)
	{
		try
		{
			std::ifstream file(metaPath);
			if (!file.is_open()) return false;

			nlohmann::json j;
			file >> j;

			if (j.contains("guid"))
			{
				if (j["guid"].is_number())
				{
					outGuid = AssetGuid(j["guid"].get<uint64_t>());
				}
				else if (j["guid"].is_string())
				{
					outGuid = AssetGuid::fromString(j["guid"].get<std::string>());
				}
			}

			if (j.contains("type") && j["type"].is_string())
			{
				outType = AssetTypeFromString(j["type"].get<std::string>());
			}

			return outGuid.isValid();
		}
		catch (...)
		{
			return false;
		}
	}

	bool AssetManager::writeMetaFile(const std::filesystem::path& metaPath, AssetGuid guid, AssetType type)
	{
		try
		{
			nlohmann::ordered_json j;
			j["guid"] = guid.value();
			j["type"] = std::string(AssetTypeToString(type));
			j["version"] = 1;

			std::ofstream file(metaPath);
			if (!file.is_open()) return false;

			file << j.dump(4);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void AssetManager::scanDirectory(const std::filesystem::path& directory)
	{
		if (!std::filesystem::exists(directory))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
		{
			if (entry.is_regular_file())
			{
				const auto& path = entry.path();
				if (path.extension() == ".meta")
				{
					continue;
				}

				registerAsset(path);
			}
		}
	}

	bool AssetManager::registerAsset(const std::filesystem::path& filePath, AssetGuid explicitGuid)
	{
		if (!std::filesystem::exists(filePath))
		{
			return false;
		}

		std::filesystem::path metaPath = filePath;
		metaPath += ".meta";

		AssetGuid guid = explicitGuid;
		AssetType type = DetectAssetTypeFromExtension(filePath);

		if (std::filesystem::exists(metaPath))
		{
			AssetGuid loadedGuid;
			AssetType loadedType;
			if (readMetaFile(metaPath, loadedGuid, loadedType))
			{
				guid = loadedGuid;
				if (loadedType != AssetType::Unknown)
				{
					type = loadedType;
				}
			}
		}

		if (!guid.isValid())
		{
			guid = AssetGuid();
			writeMetaFile(metaPath, guid, type);
		}

		std::string normPath = normalizePath(filePath);

		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		AssetMetadata meta;
		meta.guid = guid;
		meta.type = type;
		meta.filePath = filePath;
		meta.isLoaded = false;

		m_assets[guid] = meta;
		m_pathToGuid[normPath] = guid;

		return true;
	}

	bool AssetManager::hasAsset(AssetGuid guid) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		return m_assets.find(guid) != m_assets.end();
	}

	bool AssetManager::hasAsset(const std::filesystem::path& path) const
	{
		std::string normPath = normalizePath(path);
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		return m_pathToGuid.find(normPath) != m_pathToGuid.end();
	}

	const AssetMetadata* AssetManager::getMetadata(AssetGuid guid) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		auto it = m_assets.find(guid);
		if (it != m_assets.end())
		{
			return &it->second;
		}
		return nullptr;
	}

	const AssetMetadata* AssetManager::getMetadata(const std::filesystem::path& path) const
	{
		std::string normPath = normalizePath(path);
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		auto it = m_pathToGuid.find(normPath);
		if (it != m_pathToGuid.end())
		{
			auto assetIt = m_assets.find(it->second);
			if (assetIt != m_assets.end())
			{
				return &assetIt->second;
			}
		}
		return nullptr;
	}

	AssetGuid AssetManager::getGuidFromPath(const std::filesystem::path& path) const
	{
		std::string normPath = normalizePath(path);
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		auto it = m_pathToGuid.find(normPath);
		if (it != m_pathToGuid.end())
		{
			return it->second;
		}
		return AssetGuid::Empty;
	}

	std::filesystem::path AssetManager::getPathFromGuid(AssetGuid guid) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		auto it = m_assets.find(guid);
		if (it != m_assets.end())
		{
			return it->second.filePath;
		}
		return {};
	}

	bool AssetManager::renameAsset(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);

		std::string oldNorm = normalizePath(oldPath);
		std::string newNorm = normalizePath(newPath);

		auto it = m_pathToGuid.find(oldNorm);
		if (it == m_pathToGuid.end())
		{
			return false;
		}

		AssetGuid guid = it->second;

		std::filesystem::path oldMeta = oldPath;
		oldMeta += ".meta";

		std::filesystem::path newMeta = newPath;
		newMeta += ".meta";

		try
		{
			if (std::filesystem::exists(oldPath))
			{
				std::filesystem::rename(oldPath, newPath);
			}
			if (std::filesystem::exists(oldMeta))
			{
				std::filesystem::rename(oldMeta, newMeta);
			}
		}
		catch (const std::exception& e)
		{
			Log::Get().error("AssetManager", "Failed to rename asset {} to {}: {}", oldPath.string(), newPath.string(), e.what());
			return false;
		}

		m_pathToGuid.erase(oldNorm);
		m_pathToGuid[newNorm] = guid;
		m_assets[guid].filePath = newPath;

		Log::Get().info("AssetManager", "Renamed asset from {} to {} (GUID: {})", oldPath.string(), newPath.string(), guid.value());
		return true;
	}

	bool AssetManager::moveAsset(const std::filesystem::path& oldPath, const std::filesystem::path& newDirectory)
	{
		std::filesystem::path newPath = newDirectory / oldPath.filename();
		return renameAsset(oldPath, newPath);
	}

	bool AssetManager::deleteAsset(const std::filesystem::path& path)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);

		std::string normPath = normalizePath(path);
		auto it = m_pathToGuid.find(normPath);
		if (it == m_pathToGuid.end())
		{
			return false;
		}

		AssetGuid guid = it->second;

		std::filesystem::path metaPath = path;
		metaPath += ".meta";

		try
		{
			if (std::filesystem::exists(path))
			{
				std::filesystem::remove(path);
			}
			if (std::filesystem::exists(metaPath))
			{
				std::filesystem::remove(metaPath);
			}
		}
		catch (...)
		{
			return false;
		}

		m_pathToGuid.erase(normPath);
		m_assets.erase(guid);
		return true;
	}

	std::future<bool> AssetManager::loadAssetAsync(AssetGuid guid)
	{
		// Spawn async task on thread pool
		return std::async(std::launch::async, [this, guid]() -> bool {
			std::filesystem::path filePath;
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutex);
				auto it = m_assets.find(guid);
				if (it == m_assets.end())
				{
					return false;
				}
				filePath = it->second.filePath;
			}

			if (!std::filesystem::exists(filePath))
			{
				return false;
			}

			// Read file content (CPU-side decode)
			std::ifstream file(filePath, std::ios::binary | std::ios::ate);
			if (!file.is_open())
			{
				return false;
			}

			std::streamsize size = file.tellg();
			file.seekg(0, std::ios::beg);
			std::vector<char> buffer(static_cast<size_t>(size));
			if (!file.read(buffer.data(), size))
			{
				return false;
			}

			// Mark loaded in metadata
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutex);
				auto it = m_assets.find(guid);
				if (it != m_assets.end())
				{
					it->second.isLoaded = true;
				}
			}

			return true;
		});
	}

	std::vector<AssetMetadata> AssetManager::getAllAssets() const
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		std::vector<AssetMetadata> result;
		result.reserve(m_assets.size());
		for (const auto& [guid, meta] : m_assets)
		{
			result.push_back(meta);
		}
		return result;
	}

	std::vector<AssetMetadata> AssetManager::getAssetsOfType(AssetType type) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		std::vector<AssetMetadata> result;
		for (const auto& [guid, meta] : m_assets)
		{
			if (meta.type == type)
			{
				result.push_back(meta);
			}
		}
		return result;
	}

	size_t AssetManager::getAssetCount() const
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		return m_assets.size();
	}
}
