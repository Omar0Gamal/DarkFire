#pragma once

#include "Engine/Runtime/Layer.h"
#include "Engine/Runtime/Config.h"
#include "Engine/Runtime/GameModuleLoader.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Renderer/RenderScene.h"
#include "Editor/EditorCamera.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/ContentBrowserPanel.h"
#include "Editor/Panels/ProfilerPanel.h"
#include "Editor/Commands/CommandHistory.h"
#include "Editor/SelectionContext.h"
#include "Editor/UI/DockLayout.h"
#include <memory>
#include <filesystem>
#include <vector>
#include <string>

namespace Editor
{
	enum class EditorMode
	{
		Edit = 0,
		Play,
		Simulate,
		ProjectManager
	};

	enum class PlayState
	{
		Stopped = 0,
		Playing,
		Paused
	};

	using Workspace = UI::Workspace;

	struct ProjectItem
	{
		std::string name;
		std::filesystem::path path;
		std::filesystem::path dir;
		std::string engineVersion = "2.0 (D3D11)";
		std::string lastModified;
	};

	class EditorLayer : public Runtime::Layer
	{
	public:
		EditorLayer();
		~EditorLayer() override;

		void onAttach() override;
		void onDetach() override;
		void onUpdate(float deltaTime) override;
		void onFixedUpdate(float fixedDeltaTime) override;
		void onImGuiRender() override;
		void onResize(uint32_t width, uint32_t height) override;

		// Project Management
		void discoverProjects();
		bool openProject(const std::filesystem::path& projPath);
		void closeProject();

		// Scene operations
		void newScene();
		void newSandboxScene();
		void openScene();
		void openScene(const std::filesystem::path& path);
		void saveScene();
		void saveSceneAs();

		// Playtest Controls
		void onPlay();
		void onPause();
		void onStep();
		void onSimulate();
		void onStop();
		void onPlayStandalone();

		EditorMode getEditorMode() const noexcept { return m_editorMode; }
		PlayState getPlayState() const noexcept { return m_playState; }
		Workspace getActiveWorkspace() const noexcept { return m_activeWorkspace; }
		void setInProjectManager(bool inPM) noexcept { m_inProjectManager = inPM; }

	private:
		void setupImGuiStyle();
		void resizeViewport(uint32_t width, uint32_t height);
		void resizeGameViewport(uint32_t width, uint32_t height);

		// UI Bars & Docks
		void drawProjectManager();
		void drawTopBar();
		void drawBottomBar();
		void drawSceneViewport(bool* p_open = nullptr);
		void drawGameViewport(bool* p_open = nullptr);
		void drawGizmo();
		void onViewportClick();

		// Viewport Overlays
		void drawGrid3D(const Core::Matrix& viewProj);
		void drawViewportIcons(const Core::Matrix& viewProj);
		void drawSelectionWireframes(const Core::Matrix& viewProj);
		void drawCameraPiP(Core::Vector2 viewportPos, Core::Vector2 viewportSize);
		void drawOrientationGizmo(Core::Vector2 viewportPos, Core::Vector2 viewportSize);

		void performAutosave();
		void updateWindowTitle();

		// Project & Runtime state
		bool m_inProjectManager = true;
		std::filesystem::path m_currentProjectPath;
		Runtime::ProjectConfig m_projectConfig;
		std::unique_ptr<Runtime::GameModuleLoader> m_gameModule;
		std::vector<ProjectItem> m_discoveredProjects;
		int m_selectedProjectIndex = 0;
		bool m_showNewProjectModal = false;
		char m_newProjectName[128] = "MyDarkFireGame";
		char m_newProjectPath[256] = "Projects/MyDarkFireGame";
		int m_newProjectTemplate = 0; // 0: Playground3D, 1: Empty3D
		char m_projectSearchFilter[128] = "";

		// Workspaces & Modes
		Workspace m_activeWorkspace = Workspace::Viewport3D;
		EditorMode m_editorMode = EditorMode::Edit;
		PlayState m_playState = PlayState::Stopped;

		// Scenes
		std::shared_ptr<Core::Scene> m_activeScene;
		std::shared_ptr<Core::Scene> m_editScene;
		std::filesystem::path m_currentScenePath;
		bool m_isSceneDirty = false;

		// Editor Panels & Systems
		EditorCamera m_editorCamera;
		HierarchyPanel m_hierarchyPanel;
		InspectorPanel m_inspectorPanel;
		ConsolePanel m_consolePanel;
		ContentBrowserPanel m_contentBrowserPanel;
		ProfilerPanel m_profilerPanel;

		Core::Renderer::RenderScene m_renderScene;
		std::shared_ptr<Core::RHI::IGpuTexture> m_viewportTexture;
		std::shared_ptr<Core::RHI::IGpuTexture> m_gameViewportTexture;

		// Viewport state
		uint32_t m_viewportWidth = 1280;
		uint32_t m_viewportHeight = 720;
		uint32_t m_pendingViewportWidth = 0;
		uint32_t m_pendingViewportHeight = 0;
		bool m_viewportHovered = false;
		bool m_viewportFocused = false;
		bool m_focusFileSystemOnOpen = true;
		Core::Vector2 m_viewportBounds[2]{};

		uint32_t m_gameViewportWidth = 1280;
		uint32_t m_gameViewportHeight = 720;
		uint32_t m_pendingGameViewportWidth = 0;
		uint32_t m_pendingGameViewportHeight = 0;
		int m_aspectPresetIndex = 1; // 0: Free, 1: 16:9, 2: 16:10, 3: 4:3

		// Gizmo
		int m_gizmoType = 0; // 0: Translate, 1: Rotate, 2: Scale, -1: None
		int m_gizmoMode = 1; // 0: Local, 1: World
		bool m_snapEnabled = false;
		float m_snapTranslation = 1.0f;
		float m_snapRotation = 15.0f;
		float m_snapScale = 0.5f;

		// Dock visibility
		bool m_showSceneViewport = true;
		bool m_showGameViewport = true;
		bool m_showHierarchy = true;
		bool m_showInspector = true;
		bool m_showConsole = true;
		bool m_showContentBrowser = true;
		bool m_showProfiler = true;
		bool m_showStats = true;
		bool m_showWireframeMode = false;
		bool m_showGrid = true;
		bool m_showPiP = true;

		CommandHistory m_commandHistory;
		SelectionContext m_selectionContext;

		Core::TransformComponent m_gizmoInitialTransform{};
		bool m_gizmoWasUsing = false;
		float m_autosaveTimer = 0.0f;
	};
}
