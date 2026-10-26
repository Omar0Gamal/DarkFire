#include "Editor/UI/DockLayout.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdlib>

namespace Editor::UI
{
	DockLayout& DockLayout::Get()
	{
		static DockLayout s_instance;
		return s_instance;
	}

	void DockLayout::init(const std::filesystem::path& projectDir)
	{
		m_projectDirectory = projectDir;
		updateIniPath();
	}

	void DockLayout::setProjectDirectory(const std::filesystem::path& projectDir)
	{
		if (m_projectDirectory != projectDir)
		{
			// Save current settings before switching
			if (!m_activeIniPath.empty())
			{
				ImGui::SaveIniSettingsToDisk(m_activeIniPath.c_str());
			}

			m_projectDirectory = projectDir;
			updateIniPath();

			// Reload layout from new project ini
			if (!m_activeIniPath.empty() && std::filesystem::exists(m_activeIniPath))
			{
				ImGui::LoadIniSettingsFromDisk(m_activeIniPath.c_str());
			}
			else
			{
				requestResetAll();
			}
		}
	}

	void DockLayout::updateIniPath()
	{
		if (!m_projectDirectory.empty() && std::filesystem::exists(m_projectDirectory))
		{
			std::filesystem::path dotDf = m_projectDirectory / ".darkfire";
			std::error_code ec;
			std::filesystem::create_directories(dotDf, ec);
			m_activeIniPath = (dotDf / "editor_layout.ini").string();
		}
		else
		{
			// Global AppData fallback
			const char* appData = std::getenv("APPDATA");
			if (appData)
			{
				std::filesystem::path globalDir = std::filesystem::path(appData) / "DarkFire";
				std::error_code ec;
				std::filesystem::create_directories(globalDir, ec);
				m_activeIniPath = (globalDir / "editor_layout.ini").string();
			}
			else
			{
				m_activeIniPath = "editor_layout.ini";
			}
		}

		if (ImGui::GetCurrentContext())
		{
			ImGui::GetIO().IniFilename = m_activeIniPath.c_str();
		}
	}

	ImGuiID DockLayout::getDockspaceId(Workspace ws) const noexcept
	{
		switch (ws)
		{
		case Workspace::Viewport3D: return ImHashStr("DF_DockSpace_3D");
		case Workspace::Game:       return ImHashStr("DF_DockSpace_Game");
		case Workspace::Script:     return ImHashStr("DF_DockSpace_Script");
		case Workspace::Profiler:   return ImHashStr("DF_DockSpace_Profiler");
		default:                    return ImHashStr("DF_DockSpace_Default");
		}
	}

	void DockLayout::requestReset(Workspace workspace)
	{
		if (static_cast<size_t>(workspace) < static_cast<size_t>(Workspace::Count))
		{
			m_resetRequested[static_cast<size_t>(workspace)] = true;
		}
	}

	void DockLayout::requestResetAll()
	{
		for (size_t i = 0; i < static_cast<size_t>(Workspace::Count); ++i)
		{
			m_resetRequested[i] = true;
		}
	}

	void DockLayout::beginHost(Workspace activeWorkspace)
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(vp->WorkSize);
		ImGui::SetNextWindowViewport(vp->ID);

		ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		std::string hostTitle = "##EditorDockHost_" + std::to_string(static_cast<int>(activeWorkspace));
		ImGui::Begin(hostTitle.c_str(), nullptr, hostFlags);
		ImGui::PopStyleVar(3);
		m_hostActive = true;

		ImGuiID dsId = getDockspaceId(activeWorkspace);

		// Check if layout needs initial creation or reset
		ImGuiDockNode* node = ImGui::DockBuilderGetNode(dsId);
		bool needRebuild = (node == nullptr || node->IsEmpty() || m_resetRequested[static_cast<size_t>(activeWorkspace)]);

		if (needRebuild)
		{
			buildDefaultLayout(activeWorkspace, dsId);
			m_resetRequested[static_cast<size_t>(activeWorkspace)] = false;
		}

		ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
		ImGui::DockSpace(dsId, ImVec2(0.0f, 0.0f), dockFlags);
	}

	void DockLayout::endHost()
	{
		if (m_hostActive)
		{
			ImGui::End();
			m_hostActive = false;
		}
	}

	void DockLayout::buildDefaultLayout(Workspace ws, ImGuiID dockspaceId)
	{
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

		if (ws == Workspace::Viewport3D)
		{
			// Modern Layout:
			// Bottom 28%: Full-width bottom drawer with FileSystem (Assets) + Console + Profiler tabbed together
			// Top 72%: Left Scene (20%), Center Viewports (52%), Right Inspector (28%)
			ImGuiID dockBottom = 0;
			ImGuiID dockTop = 0;
			ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.28f, &dockBottom, &dockTop);

			ImGuiID dockLeft = 0;
			ImGuiID dockCenterAndRight = 0;
			ImGui::DockBuilderSplitNode(dockTop, ImGuiDir_Left, 0.20f, &dockLeft, &dockCenterAndRight);

			ImGuiID dockRight = 0;
			ImGuiID dockCenter = 0;
			ImGui::DockBuilderSplitNode(dockCenterAndRight, ImGuiDir_Right, 0.28f, &dockRight, &dockCenter);

			// Left column
			ImGui::DockBuilderDockWindow(PanelNames::Scene, dockLeft);

			// Center Viewports
			ImGui::DockBuilderDockWindow(PanelNames::Viewport3D, dockCenter);
			ImGui::DockBuilderDockWindow(PanelNames::GameViewport, dockCenter);

			// Right Inspector & World Settings
			ImGui::DockBuilderDockWindow(PanelNames::Inspector, dockRight);
			ImGui::DockBuilderDockWindow(PanelNames::WorldSettings, dockRight);

			// Bottom Drawer: FileSystem + Console + Profiler tabbed
			ImGui::DockBuilderDockWindow(PanelNames::FileSystem, dockBottom);
			ImGui::DockBuilderDockWindow(PanelNames::Console, dockBottom);
			ImGui::DockBuilderDockWindow(PanelNames::Profiler, dockBottom);

			ImGuiDockNode* bottomNode = ImGui::DockBuilderGetNode(dockBottom);
			if (bottomNode)
			{
				bottomNode->SelectedTabId = ImHashStr(PanelNames::FileSystem);
			}
		}
		else if (ws == Workspace::Game)
		{
			// Game Layout: Dominant Center Viewport, Right Inspector + Scene
			ImGuiID dockRight = 0;
			ImGuiID dockCenter = 0;
			ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

			ImGuiID dockRightTop = 0;
			ImGuiID dockRightBottom = 0;
			ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Up, 0.40f, &dockRightTop, &dockRightBottom);

			ImGuiID dockCenterTop = 0;
			ImGuiID dockCenterBottom = 0;
			ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Up, 0.78f, &dockCenterTop, &dockCenterBottom);

			ImGui::DockBuilderDockWindow(PanelNames::GameViewport, dockCenterTop);
			ImGui::DockBuilderDockWindow(PanelNames::Viewport3D, dockCenterTop);
			ImGui::DockBuilderDockWindow(PanelNames::Console, dockCenterBottom);
			ImGui::DockBuilderDockWindow(PanelNames::Scene, dockRightTop);
			ImGui::DockBuilderDockWindow(PanelNames::Inspector, dockRightBottom);
		}
		else if (ws == Workspace::Script)
		{
			// Script Layout: Left FileSystem, Center Script Editor, Bottom Console, Right Inspector
			ImGuiID dockLeft = 0;
			ImGuiID dockCenterAndRight = 0;
			ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.22f, &dockLeft, &dockCenterAndRight);

			ImGuiID dockRight = 0;
			ImGuiID dockCenter = 0;
			ImGui::DockBuilderSplitNode(dockCenterAndRight, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

			ImGuiID dockCenterTop = 0;
			ImGuiID dockCenterBottom = 0;
			ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Up, 0.75f, &dockCenterTop, &dockCenterBottom);

			ImGui::DockBuilderDockWindow(PanelNames::FileSystem, dockLeft);
			ImGui::DockBuilderDockWindow(PanelNames::Scene, dockLeft);
			ImGui::DockBuilderDockWindow(PanelNames::ScriptEditor, dockCenterTop);
			ImGui::DockBuilderDockWindow(PanelNames::Console, dockCenterBottom);
			ImGui::DockBuilderDockWindow(PanelNames::Inspector, dockRight);
		}
		else if (ws == Workspace::Profiler)
		{
			// Profiler Layout: Top Profiler Timeline, Bottom Console
			ImGuiID dockTop = 0;
			ImGuiID dockBottom = 0;
			ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Up, 0.70f, &dockTop, &dockBottom);

			ImGui::DockBuilderDockWindow(PanelNames::Profiler, dockTop);
			ImGui::DockBuilderDockWindow(PanelNames::Console, dockBottom);
		}

		ImGui::DockBuilderFinish(dockspaceId);
	}
}
