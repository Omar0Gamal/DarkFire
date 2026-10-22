#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Assets/AssetTypes.h"
#include <filesystem>
#include <string>
#include <vector>
#include <functional>

namespace Editor
{
	class ContentBrowserPanel
	{
	public:
		using OpenSceneCallback = std::function<void(const std::filesystem::path&)>;

		ContentBrowserPanel();
		explicit ContentBrowserPanel(std::filesystem::path rootDirectory);

		void setRootDirectory(const std::filesystem::path& root);
		void onImGuiRender(bool* p_open = nullptr);
		void setOpenSceneCallback(OpenSceneCallback callback) { m_openSceneCallback = std::move(callback); }
		const std::filesystem::path& getCurrentDirectory() const noexcept { return m_currentDirectory; }

	private:
		void drawDirectoryTree(const std::filesystem::path& directory);
		void drawBreadcrumbs();
		void drawFileGrid();
		void drawContextMenu();

		std::filesystem::path m_rootDirectory;
		std::filesystem::path m_currentDirectory;
		std::vector<std::filesystem::path> m_history;
		int m_historyIndex = -1;

		char m_searchFilter[128]{};
		float m_thumbnailSize = 72.0f;
		float m_padding = 12.0f;

		OpenSceneCallback m_openSceneCallback;
		std::filesystem::path m_selectedPath;
		std::filesystem::path m_renamingPath;
		char m_renameBuffer[128]{};
	};
}
