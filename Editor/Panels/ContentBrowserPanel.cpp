#include "Editor/Panels/ContentBrowserPanel.h"
#include "Editor/UI/DockLayout.h"
#include "Engine/Core/Assets/AssetManager.h"
#include "Engine/Core/Log/Log.h"
#include <imgui.h>
#include <algorithm>
#include <fstream>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

namespace Editor
{
	ContentBrowserPanel::ContentBrowserPanel()
		: ContentBrowserPanel("Templates/Playground3D/Assets")
	{
	}

	ContentBrowserPanel::ContentBrowserPanel(std::filesystem::path rootDirectory)
		: m_rootDirectory(std::move(rootDirectory))
	{
		if (!std::filesystem::exists(m_rootDirectory))
		{
			// Try fallback directory
			if (std::filesystem::exists("Engine/Content"))
			{
				m_rootDirectory = "Engine/Content";
			}
			else
			{
				m_rootDirectory = ".";
			}
		}

		m_currentDirectory = m_rootDirectory;
		m_history.push_back(m_currentDirectory);
		m_historyIndex = 0;
	}

	void ContentBrowserPanel::setRootDirectory(const std::filesystem::path& root)
	{
		m_rootDirectory = root;
		if (!std::filesystem::exists(m_rootDirectory))
		{
			std::filesystem::create_directories(m_rootDirectory);
		}
		m_currentDirectory = m_rootDirectory;
		m_history.clear();
		m_history.push_back(m_currentDirectory);
		m_historyIndex = 0;
	}

	void ContentBrowserPanel::onImGuiRender(bool* p_open)
	{
		ImGui::Begin(UI::PanelNames::FileSystem, p_open);

		// Header navigation bar
		drawBreadcrumbs();
		ImGui::Separator();

		// Two column layout: Tree view on left, Asset grid on right
		ImGui::Columns(2, "ContentBrowserColumns", true);
		static bool initialSet = false;
		if (!initialSet)
		{
			ImGui::SetColumnWidth(0, 200.0f);
			initialSet = true;
		}

		// Left: Folder Tree
		ImGui::BeginChild("FolderTreeChild", ImVec2(0, 0), false);
		drawDirectoryTree(m_rootDirectory);
		if (std::filesystem::exists("Engine/Content") && m_rootDirectory != "Engine/Content")
		{
			drawDirectoryTree("Engine/Content");
		}
		ImGui::EndChild();

		ImGui::NextColumn();

		// Right: File Grid
		ImGui::BeginChild("FileGridChild", ImVec2(0, 0), false);
		drawFileGrid();
		ImGui::EndChild();

		ImGui::Columns(1);
		ImGui::End();
	}

	void ContentBrowserPanel::drawBreadcrumbs()
	{
		// Back button
		bool canGoBack = m_historyIndex > 0;
		if (!canGoBack) ImGui::BeginDisabled();
		if (ImGui::Button("<"))
		{
			if (canGoBack)
			{
				--m_historyIndex;
				m_currentDirectory = m_history[m_historyIndex];
			}
		}
		if (!canGoBack) ImGui::EndDisabled();

		ImGui::SameLine();

		// Forward button
		bool canGoForward = m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_history.size()) - 1;
		if (!canGoForward) ImGui::BeginDisabled();
		if (ImGui::Button(">"))
		{
			if (canGoForward)
			{
				++m_historyIndex;
				m_currentDirectory = m_history[m_historyIndex];
			}
		}
		if (!canGoForward) ImGui::EndDisabled();

		ImGui::SameLine();

		// Breadcrumbs path elements
		std::filesystem::path accumPath;
		int id = 0;
		for (const auto& part : m_currentDirectory)
		{
			if (id > 0)
			{
				accumPath /= part;
				ImGui::TextDisabled("/");
				ImGui::SameLine();
			}
			else
			{
				accumPath = part;
			}

			ImGui::PushID(id++);
			std::string label = part.string();
			if (ImGui::SmallButton(label.c_str()))
			{
				m_currentDirectory = accumPath;
				m_history.erase(m_history.begin() + (m_historyIndex + 1), m_history.end());
				m_history.push_back(m_currentDirectory);
				m_historyIndex = static_cast<int>(m_history.size()) - 1;
			}
			ImGui::PopID();
			ImGui::SameLine();
		}

		// Search Filter
		float searchWidth = 160.0f;
		float availX = ImGui::GetContentRegionAvail().x;
		if (availX > searchWidth + 20.0f)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availX - searchWidth));
			ImGui::SetNextItemWidth(searchWidth);
			ImGui::InputTextWithHint("##AssetSearch", "Search...", m_searchFilter, sizeof(m_searchFilter));
		}
	}

	void ContentBrowserPanel::drawDirectoryTree(const std::filesystem::path& directory)
	{
		if (!std::filesystem::exists(directory))
		{
			return;
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (m_currentDirectory == directory)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		bool hasSubdirs = false;
		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			if (entry.is_directory())
			{
				hasSubdirs = true;
				break;
			}
		}

		if (!hasSubdirs)
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		std::string folderName = directory.filename().string();
		if (folderName.empty()) folderName = directory.string();

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)std::hash<std::string>{}(directory.generic_string()), flags, "%s", folderName.c_str());

		if (ImGui::IsItemClicked())
		{
			m_currentDirectory = directory;
			m_history.erase(m_history.begin() + (m_historyIndex + 1), m_history.end());
			m_history.push_back(m_currentDirectory);
			m_historyIndex = static_cast<int>(m_history.size()) - 1;
		}

		if (opened && hasSubdirs)
		{
			for (const auto& entry : std::filesystem::directory_iterator(directory))
			{
				if (entry.is_directory())
				{
					drawDirectoryTree(entry.path());
				}
			}
			ImGui::TreePop();
		}
	}

	static ImVec4 GetAssetColor(Core::AssetType type)
	{
		using namespace Core;
		switch (type)
		{
		case AssetType::Texture:  return ImVec4(0.85f, 0.35f, 0.45f, 1.0f); // Rose
		case AssetType::Mesh:     return ImVec4(0.25f, 0.65f, 0.95f, 1.0f); // Blue
		case AssetType::Material: return ImVec4(0.95f, 0.75f, 0.20f, 1.0f); // Amber
		case AssetType::Shader:   return ImVec4(0.30f, 0.85f, 0.55f, 1.0f); // Emerald
		case AssetType::Scene:    return ImVec4(0.65f, 0.40f, 0.90f, 1.0f); // Purple
		case AssetType::Prefab:   return ImVec4(0.20f, 0.85f, 0.85f, 1.0f); // Cyan
		default:                  return ImVec4(0.50f, 0.50f, 0.55f, 1.0f); // Gray
		}
	}

	void ContentBrowserPanel::drawFileGrid()
	{
		// Context menu on empty space
		if (ImGui::BeginPopupContextWindow("ContentBrowserGridContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			drawContextMenu();
			ImGui::EndPopup();
		}

		if (!std::filesystem::exists(m_currentDirectory))
		{
			ImGui::TextDisabled("Directory does not exist.");
			return;
		}

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = static_cast<int>(panelWidth / (m_thumbnailSize + m_padding));
		if (columnCount < 1) columnCount = 1;

		ImGui::Columns(columnCount, nullptr, false);

		int id = 0;
		for (const auto& entry : std::filesystem::directory_iterator(m_currentDirectory))
		{
			const auto& path = entry.path();
			std::string filename = path.filename().string();

			// Skip meta files in grid
			if (path.extension() == ".meta")
			{
				continue;
			}

			// Filter check
			if (m_searchFilter[0] != '\0')
			{
				std::string lowerName = filename;
				std::string lowerFilter = m_searchFilter;
				std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
				std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
				if (lowerName.find(lowerFilter) == std::string::npos)
				{
					continue;
				}
			}

			ImGui::PushID(id++);

			bool isDir = entry.is_directory();
			Core::AssetType assetType = isDir ? Core::AssetType::Unknown : Core::DetectAssetTypeFromExtension(path);

			// Render icon button
			ImVec4 color = isDir ? ImVec4(0.95f, 0.80f, 0.30f, 1.0f) : GetAssetColor(assetType);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 0.8f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.7f, color.y * 0.7f, color.z * 0.7f, 1.0f));

			const char* iconBadge = isDir ? "[DIR]" : (assetType == Core::AssetType::Texture ? "[TEX]" : (assetType == Core::AssetType::Mesh ? "[MESH]" : (assetType == Core::AssetType::Material ? "[MAT]" : (assetType == Core::AssetType::Scene ? "[SCENE]" : (assetType == Core::AssetType::Prefab ? "[PREFAB]" : "[FILE]")))));

			ImGui::Button(iconBadge, ImVec2(m_thumbnailSize, m_thumbnailSize));
			ImGui::PopStyleColor(3);

			// Drag & Drop Source
			if (!isDir && ImGui::BeginDragDropSource())
			{
				std::string pathStr = path.generic_string();
				ImGui::SetDragDropPayload("DARKFIRE_ASSET_PATH", pathStr.c_str(), pathStr.size() + 1);
				ImGui::Text("%s", filename.c_str());
				ImGui::EndDragDropSource();
			}

			// Double-click navigation / open
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				if (isDir)
				{
					m_currentDirectory = path;
					m_history.erase(m_history.begin() + (m_historyIndex + 1), m_history.end());
					m_history.push_back(m_currentDirectory);
					m_historyIndex = static_cast<int>(m_history.size()) - 1;
				}
				else if (path.extension() == ".scene")
				{
					if (m_openSceneCallback)
					{
						m_openSceneCallback(path);
					}
				}
			}

			// Context menu on item
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Rename"))
				{
					m_renamingPath = path;
					strncpy_s(m_renameBuffer, filename.c_str(), sizeof(m_renameBuffer) - 1);
				}
				if (ImGui::MenuItem("Delete"))
				{
					if (isDir)
					{
						std::filesystem::remove_all(path);
					}
					else
					{
						Core::AssetManager::Get().deleteAsset(path);
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Show in Explorer"))
				{
					ShellExecuteA(nullptr, "explore", m_currentDirectory.string().c_str(), nullptr, nullptr, SW_SHOW);
				}
				ImGui::EndPopup();
			}

			// File label or inline rename input
			if (m_renamingPath == path)
			{
				ImGui::SetNextItemWidth(m_thumbnailSize);
				if (ImGui::InputText("##RenameInput", m_renameBuffer, sizeof(m_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
				{
					std::filesystem::path newPath = path.parent_path() / m_renameBuffer;
					if (isDir)
					{
						std::filesystem::rename(path, newPath);
					}
					else
					{
						Core::AssetManager::Get().renameAsset(path, newPath);
					}
					m_renamingPath.clear();
				}
				if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))
				{
					m_renamingPath.clear();
				}
			}
			else
			{
				ImGui::TextWrapped("%s", filename.c_str());
			}

			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);
	}

	void ContentBrowserPanel::drawContextMenu()
	{
		if (ImGui::MenuItem("New Folder"))
		{
			std::filesystem::path newDir = m_currentDirectory / "NewFolder";
			int counter = 1;
			while (std::filesystem::exists(newDir))
			{
				newDir = m_currentDirectory / ("NewFolder_" + std::to_string(counter++));
			}
			std::filesystem::create_directory(newDir);
		}

		if (ImGui::MenuItem("New Material"))
		{
			std::filesystem::path matPath = m_currentDirectory / "NewMaterial.mat";
			int counter = 1;
			while (std::filesystem::exists(matPath))
			{
				matPath = m_currentDirectory / ("NewMaterial_" + std::to_string(counter++) + ".mat");
			}

			std::ofstream matFile(matPath);
			if (matFile.is_open())
			{
				matFile << "{\n  \"name\": \"NewMaterial\",\n  \"baseColor\": [1.0, 1.0, 1.0, 1.0],\n  \"roughness\": 0.5,\n  \"metallic\": 0.0,\n  \"emissive\": [0.0, 0.0, 0.0, 0.0],\n  \"tiling\": [1.0, 1.0],\n  \"albedoTexture\": \"\",\n  \"normalTexture\": \"\",\n  \"metallicRoughnessTexture\": \"\",\n  \"emissiveTexture\": \"\",\n  \"blendMode\": 0,\n  \"depthMode\": 0,\n  \"cullMode\": 1\n}\n";
			}
			Core::AssetManager::Get().registerAsset(matPath);
		}

		if (ImGui::MenuItem("New Scene"))
		{
			std::filesystem::path scenePath = m_currentDirectory / "NewScene.scene";
			int counter = 1;
			while (std::filesystem::exists(scenePath))
			{
				scenePath = m_currentDirectory / ("NewScene_" + std::to_string(counter++) + ".scene");
			}

			std::ofstream scFile(scenePath);
			if (scFile.is_open())
			{
				scFile << "{\n  \"scene_version\": 1,\n  \"entities\": []\n}\n";
			}
			Core::AssetManager::Get().registerAsset(scenePath);
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Show in Explorer"))
		{
			ShellExecuteA(nullptr, "explore", m_currentDirectory.string().c_str(), nullptr, nullptr, SW_SHOW);
		}
	}
}
