#pragma once

#include <imgui.h>
#include <string>
#include <filesystem>
#include <cstdint>

namespace Editor::UI
{
	enum class Workspace : uint8_t
	{
		Viewport3D = 0,
		Game,
		Script,
		Profiler,
		Count
	};

	namespace PanelNames
	{
		inline constexpr const char* Scene        = "Scene###DF_Scene";
		inline constexpr const char* FileSystem   = "FileSystem###DF_FileSystem";
		inline constexpr const char* Viewport3D   = "3D Viewport###DF_Viewport3D";
		inline constexpr const char* GameViewport = "Game Viewport###DF_GameView";
		inline constexpr const char* Inspector    = "Inspector###DF_Inspector";
		inline constexpr const char* WorldSettings = "World Settings###DF_WorldSettings";
		inline constexpr const char* Console      = "Console###DF_Console";
		inline constexpr const char* Profiler     = "Profiler###DF_Profiler";
		inline constexpr const char* ScriptEditor = "Script Editor###DF_ScriptEditor";
	}

	class DockLayout
	{
	public:
		static DockLayout& Get();

		void init(const std::filesystem::path& projectDir = {});
		void setProjectDirectory(const std::filesystem::path& projectDir);

		void beginHost(Workspace activeWorkspace);
		void endHost();

		void requestReset(Workspace workspace);
		void requestResetAll();

		const std::string& getIniPath() const noexcept { return m_activeIniPath; }
		ImGuiID getDockspaceId(Workspace ws) const noexcept;

	private:
		DockLayout() = default;
		void updateIniPath();
		void buildDefaultLayout(Workspace ws, ImGuiID dockspaceId);

		std::filesystem::path m_projectDirectory;
		std::string m_activeIniPath;
		bool m_resetRequested[static_cast<size_t>(Workspace::Count)]{};
		bool m_hostActive = false;
	};
}
