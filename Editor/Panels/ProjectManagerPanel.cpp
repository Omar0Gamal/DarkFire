#include "Editor/Panels/ProjectManagerPanel.h"
#include "Editor/FileDialogs.h"
#include <imgui.h>
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <algorithm>

namespace Editor
{
	void ProjectManagerPanel::discoverProjects()
	{
		m_discoveredProjects.clear();

		auto scanDir = [&](const std::filesystem::path& dir) {
			if (!std::filesystem::exists(dir)) return;

			for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".dfproj")
				{
					ProjectItem item;
					item.path = entry.path();
					item.dir = entry.path().parent_path();
					item.name = entry.path().parent_path().filename().string();

					bool exists = false;
					for (const auto& p : m_discoveredProjects)
					{
						if (p.path == item.path) { exists = true; break; }
					}
					if (!exists)
					{
						m_discoveredProjects.push_back(item);
					}
				}
			}
		};

		scanDir("Templates");
		scanDir("Projects");
		scanDir(FileDialogs::GetDefaultProjectsDirectory());

		if (m_discoveredProjects.empty())
		{
			if (std::filesystem::exists("Templates/Playground3D/Project.dfproj"))
			{
				ProjectItem p;
				p.name = "Playground3D";
				p.path = "Templates/Playground3D/Project.dfproj";
				p.dir = "Templates/Playground3D";
				m_discoveredProjects.push_back(p);
			}
			if (std::filesystem::exists("Templates/Empty3D/Project.dfproj"))
			{
				ProjectItem p;
				p.name = "Empty3D";
				p.path = "Templates/Empty3D/Project.dfproj";
				p.dir = "Templates/Empty3D";
				m_discoveredProjects.push_back(p);
			}
			if (std::filesystem::exists("Templates/GraphicsTest3D/Project.dfproj"))
			{
				ProjectItem p;
				p.name = "GraphicsTest3D";
				p.path = "Templates/GraphicsTest3D/Project.dfproj";
				p.dir = "Templates/GraphicsTest3D";
				m_discoveredProjects.push_back(p);
			}
		}
	}

	void ProjectManagerPanel::draw()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
		ImGui::Begin("##ProjectManagerScreen", nullptr, flags);

		// Header Banner
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.13f, 1.0f));
		ImGui::BeginChild("##PMHeader", ImVec2(0, 70.0f), false);
		ImGui::SetCursorPos(ImVec2(20.0f, 15.0f));
		ImGui::TextColored(ImVec4(0.278f, 0.549f, 0.749f, 1.0f), "DarkFire Engine 2.0");
		ImGui::SameLine();
		ImGui::TextDisabled("— DirectX 11 Reversed-Z Modern Engine");

		ImGui::SetCursorPos(ImVec2(20.0f, 38.0f));
		ImGui::TextDisabled("Godot 4-style Project Manager");
		ImGui::EndChild();
		ImGui::PopStyleColor();

		// Action Bar
		ImGui::SetCursorPos(ImVec2(20.0f, 85.0f));
		if (ImGui::Button("+ New Project...", ImVec2(130.0f, 28.0f)))
		{
			m_showNewProjectModal = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Import / Browse...", ImVec2(130.0f, 28.0f)))
		{
			std::string p = FileDialogs::OpenFile("DarkFire Project (*.dfproj)\0*.dfproj\0\0");
			if (!p.empty() && m_onOpenProject)
			{
				m_onOpenProject(p);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh", ImVec2(80.0f, 28.0f)))
		{
			std::filesystem::path defPath = FileDialogs::GetDefaultProjectsDirectory() / m_newProjectName;
			strncpy_s(m_newProjectPath, defPath.string().c_str(), sizeof(m_newProjectPath) - 1);
			discoverProjects();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(300.0f);
		ImGui::InputTextWithHint("##ProjFilter", "Filter projects...", m_projectSearchFilter, sizeof(m_projectSearchFilter));

		ImGui::Separator();

		// Two Column Layout: Project List (75%) vs Actions (25%)
		float contentHeight = viewport->Size.y - 140.0f;
		ImGui::Columns(2, "PMColumns", true);
		ImGui::SetColumnWidth(0, viewport->Size.x * 0.75f);

		// Column 0: Project List
		ImGui::BeginChild("##ProjectListChild", ImVec2(0, contentHeight), true);
		std::string filterStr = m_projectSearchFilter;
		std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

		for (int i = 0; i < static_cast<int>(m_discoveredProjects.size()); ++i)
		{
			const auto& proj = m_discoveredProjects[i];
			std::string pNameLower = proj.name;
			std::transform(pNameLower.begin(), pNameLower.end(), pNameLower.begin(), ::tolower);

			if (!filterStr.empty() && pNameLower.find(filterStr) == std::string::npos)
			{
				continue;
			}

			ImGui::PushID(i);
			bool isSelected = (m_selectedProjectIndex == i);

			if (isSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.23f, 0.28f, 1.0f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.16f, 0.19f, 0.5f));
			}

			ImGui::BeginChild("##ItemBox", ImVec2(0, 54.0f), true);

			// Project Name
			ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.98f, 1.0f), "%s", proj.name.c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("(%s)", proj.engineVersion.c_str());

			// Project Path
			ImGui::TextDisabled("%s", proj.dir.string().c_str());

			ImGui::EndChild();
			ImGui::PopStyleColor();

			if (ImGui::IsItemClicked())
			{
				m_selectedProjectIndex = i;
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				m_selectedProjectIndex = i;
				if (m_onOpenProject)
				{
					m_onOpenProject(proj.path);
				}
			}

			ImGui::PopID();
		}
		ImGui::EndChild();

		// Column 1: Action Sidebar
		ImGui::NextColumn();
		ImGui::BeginChild("##PMActionsChild", ImVec2(0, contentHeight), false);

		if (!m_discoveredProjects.empty() && m_selectedProjectIndex >= 0 && m_selectedProjectIndex < static_cast<int>(m_discoveredProjects.size()))
		{
			const auto& selProj = m_discoveredProjects[m_selectedProjectIndex];

			ImGui::TextColored(ImVec4(0.278f, 0.549f, 0.749f, 1.0f), "%s", selProj.name.c_str());
			ImGui::TextWrapped("%s", selProj.path.string().c_str());
			ImGui::Separator();

			// Prominent Edit Button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.278f, 0.549f, 0.749f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.357f, 0.624f, 0.831f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			if (ImGui::Button("Edit Project", ImVec2(-1, 36.0f)))
			{
				if (m_onOpenProject)
				{
					m_onOpenProject(selProj.path);
				}
			}
			ImGui::PopStyleColor(3);

			ImGui::Spacing();
			if (ImGui::Button("Run Standalone", ImVec2(-1, 28.0f)))
			{
				if (m_onPlayStandalone)
				{
					m_onPlayStandalone(selProj.path);
				}
			}

			if (ImGui::Button("Show in Explorer", ImVec2(-1, 28.0f)))
			{
				ShellExecuteA(nullptr, "explore", selProj.dir.string().c_str(), nullptr, nullptr, SW_SHOW);
			}

			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button("Remove from List", ImVec2(-1, 26.0f)))
			{
				m_discoveredProjects.erase(m_discoveredProjects.begin() + m_selectedProjectIndex);
				if (m_selectedProjectIndex >= static_cast<int>(m_discoveredProjects.size()))
				{
					m_selectedProjectIndex = static_cast<int>(m_discoveredProjects.size()) - 1;
				}
			}
		}
		else
		{
			ImGui::TextDisabled("Select a project from the list to edit or run.");
		}

		ImGui::EndChild();
		ImGui::Columns(1);

		// New Project Modal Wizard
		if (m_showNewProjectModal)
		{
			ImGui::OpenPopup("Create New DarkFire Project");
		}

		if (ImGui::BeginPopupModal("Create New DarkFire Project", &m_showNewProjectModal, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Project Name:");
			ImGui::SetNextItemWidth(350.0f);
			if (ImGui::InputText("##NewProjName", m_newProjectName, sizeof(m_newProjectName)))
			{
				std::filesystem::path currentBase = std::filesystem::path(m_newProjectPath).parent_path();
				if (currentBase.empty() || currentBase == "Projects")
				{
					currentBase = FileDialogs::GetDefaultProjectsDirectory();
				}
				std::filesystem::path p = currentBase / m_newProjectName;
				strncpy_s(m_newProjectPath, p.string().c_str(), sizeof(m_newProjectPath) - 1);
			}

			ImGui::Text("Project Directory:");
			ImGui::SetNextItemWidth(300.0f);
			ImGui::InputText("##NewProjPath", m_newProjectPath, sizeof(m_newProjectPath));
			ImGui::SameLine();
			if (ImGui::Button("Browse..."))
			{
				std::string folder = FileDialogs::SelectFolder();
				if (!folder.empty())
				{
					std::filesystem::path p = std::filesystem::path(folder) / m_newProjectName;
					strncpy_s(m_newProjectPath, p.string().c_str(), sizeof(m_newProjectPath) - 1);
				}
			}

			ImGui::Text("Template:");
			ImGui::RadioButton("Playground3D (Sample Physics & Scripts)", &m_newProjectTemplate, 0);
			ImGui::RadioButton("Empty3D (Clean Blank Slate)", &m_newProjectTemplate, 1);
			ImGui::RadioButton("GraphicsTest3D (AAA Sponza & Tropical Island Benchmark)", &m_newProjectTemplate, 2);

			ImGui::Separator();

			if (ImGui::Button("Create & Edit", ImVec2(130, 0)))
			{
				std::filesystem::path destDir = m_newProjectPath;
				std::filesystem::create_directories(destDir / "Scenes");
				std::filesystem::create_directories(destDir / "Assets");
				std::filesystem::create_directories(destDir / "Source");

				std::filesystem::path dfproj = destDir / "Project.dfproj";
				{
					std::ofstream f(dfproj);
					if (f.is_open())
					{
						std::string tmplName = (m_newProjectTemplate == 2) ? "GraphicsTest3D" : ((m_newProjectTemplate == 0) ? "Playground3D" : "Empty3D");
						std::string startScene = (m_newProjectTemplate == 2) ? "Scenes/SponzaAtrium.scene" : "Scenes/Start.scene";
						f << "{\n"
						  << "  \"project_version\": 2,\n"
						  << "  \"name\": \"" << m_newProjectName << "\",\n"
						  << "  \"start_scene\": \"" << startScene << "\",\n"
						  << "  \"module_path\": \"bin/%CONFIG%-windows-x86_64/Templates/" << tmplName << "/" << tmplName << ".dll\"\n"
						  << "}\n";
						f.flush();
						f.close();
					}
				}

				m_showNewProjectModal = false;
				ImGui::CloseCurrentPopup();

				std::filesystem::path defPath = FileDialogs::GetDefaultProjectsDirectory() / m_newProjectName;
				strncpy_s(m_newProjectPath, defPath.string().c_str(), sizeof(m_newProjectPath) - 1);
				discoverProjects();

				if (m_onOpenProject)
				{
					m_onOpenProject(dfproj);
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(80, 0)))
			{
				m_showNewProjectModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::End();
		ImGui::PopStyleVar(2);
	}
}
