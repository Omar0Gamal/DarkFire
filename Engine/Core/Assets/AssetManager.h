#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Assets/AssetGuid.h"
#include "Engine/Core/Assets/AssetTypes.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <mutex>
#include <future>
#include <vector>

namespace Core
{
	class DF_API AssetManager
	{
	public:
		static AssetManager& Get();

		AssetManager() = default;
		~AssetManager() = default;

		AssetManager(const AssetManager&) = delete;
		AssetManager& operator=(const AssetManager&) = delete;

		void init(const std::filesystem::path& projectRoot = "");
		void shutdown();

		void scanDirectory(const std::filesystem::path& directory);

		bool hasAsset(AssetGuid guid) const;
		bool hasAsset(const std::filesystem::path& path) const;

		const AssetMetadata* getMetadata(AssetGuid guid) const;
		const AssetMetadata* getMetadata(const std::filesystem::path& path) const;

		AssetGuid getGuidFromPath(const std::filesystem::path& path) const;
		std::filesystem::path getPathFromGuid(AssetGuid guid) const;

		bool registerAsset(const std::filesystem::path& filePath, AssetGuid explicitGuid = AssetGuid::Empty);

		bool renameAsset(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
		bool moveAsset(const std::filesystem::path& oldPath, const std::filesystem::path& newDirectory);
		bool deleteAsset(const std::filesystem::path& path);

		std::future<bool> loadAssetAsync(AssetGuid guid);

		std::vector<AssetMetadata> getAllAssets() const;
		std::vector<AssetMetadata> getAssetsOfType(AssetType type) const;

		size_t getAssetCount() const;

	private:
		bool readMetaFile(const std::filesystem::path& metaPath, AssetGuid& outGuid, AssetType& outType);
		bool writeMetaFile(const std::filesystem::path& metaPath, AssetGuid guid, AssetType type);
		std::string normalizePath(const std::filesystem::path& path) const;

		mutable std::recursive_mutex m_mutex;
		std::unordered_map<AssetGuid, AssetMetadata> m_assets;
		std::unordered_map<std::string, AssetGuid> m_pathToGuid;
		std::filesystem::path m_projectRoot;
		bool m_initialized = false;
	};
}
