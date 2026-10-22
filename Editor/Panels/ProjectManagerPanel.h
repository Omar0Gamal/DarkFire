#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace Editor
{
	struct ProjectItem
	{
		std::string name;
		std::filesystem::path path;
		std::filesystem::path dir;
		std::string engineVersion = "2.0 (D3D11)";
		std::string lastModified;
	};

	class ProjectManagerPanel
	{
	public:
		ProjectManagerPanel() = default;

		void discoverProjects();
		void draw();

		void setOnOpenProject(std::function<bool(const std::filesystem::path&)> callback) { m_onOpenProject = std::move(callback); }
		void setOnPlayStandalone(std::function<void(const std::filesystem::path&)> callback) { m_onPlayStandalone = std::move(callback); }

		const std::vector<ProjectItem>& getDiscoveredProjects() const noexcept { return m_discoveredProjects; }
		int getSelectedProjectIndex() const noexcept { return m_selectedProjectIndex; }

	private:
		std::vector<ProjectItem> m_discoveredProjects;
		int m_selectedProjectIndex = 0;
		bool m_showNewProjectModal = false;
		char m_newProjectName[128] = "MyDarkFireGame";
		char m_newProjectPath[256] = "Projects/MyDarkFireGame";
		int m_newProjectTemplate = 0; // 0: Playground3D, 1: Empty3D
		char m_projectSearchFilter[128] = "";

		std::function<bool(const std::filesystem::path&)> m_onOpenProject;
		std::function<void(const std::filesystem::path&)> m_onPlayStandalone;
	};
}
