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
#include "Editor/Panels/ProjectManagerPanel.h"
#include "Editor/Panels/WorldSettingsPanel.h"
#include "Editor/Panels/ProjectSettingsPanel.h"
#include "Editor/Panels/PackagingModal.h"
#include "Editor/Scripting/ScriptWizardModal.h"
#include "Editor/Viewport/SceneViewportPanel.h"
#include "Editor/Viewport/GameViewportPanel.h"
#include "Editor/PlaySession.h"
#include "Editor/Commands/CommandHistory.h"
#include "Editor/SelectionContext.h"
#include "Editor/UI/DockLayout.h"
#include <memory>
#include <filesystem>
#include <vector>
#include <string>

namespace Editor
{
	using Workspace = UI::Workspace;

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
		void buildAndReloadGameModule();

		EditorMode getEditorMode() const noexcept { return m_playSession.getEditorMode(); }
		PlayState getPlayState() const noexcept { return m_playSession.getPlayState(); }
		Workspace getActiveWorkspace() const noexcept { return m_activeWorkspace; }
		void setInProjectManager(bool inPM) noexcept { m_inProjectManager = inPM; }

	private:
		void setupImGuiStyle();
		void resizeViewport(uint32_t width, uint32_t height);
		void resizeGameViewport(uint32_t width, uint32_t height);

		// UI Bars
		void drawTopBar();
		void drawBottomBar();

		void performAutosave();
		void updateWindowTitle();

		// Project & Runtime state
		bool m_inProjectManager = true;
		std::filesystem::path m_currentProjectPath;
		Runtime::ProjectConfig m_projectConfig;
		std::unique_ptr<Runtime::GameModuleLoader> m_gameModule;

		// Workspaces & Sessions
		Workspace m_activeWorkspace = Workspace::Viewport3D;
		PlaySession m_playSession;

		// Scenes
		std::shared_ptr<Core::Scene> m_activeScene;
		std::filesystem::path m_currentScenePath;
		bool m_isSceneDirty = false;

		// Editor Panels & Systems
		EditorCamera m_editorCamera;
		HierarchyPanel m_hierarchyPanel;
		InspectorPanel m_inspectorPanel;
		ConsolePanel m_consolePanel;
		ContentBrowserPanel m_contentBrowserPanel;
		ProfilerPanel m_profilerPanel;
		ProjectManagerPanel m_projectManagerPanel;
		WorldSettingsPanel m_worldSettingsPanel;
		ProjectSettingsPanel m_projectSettingsPanel;
		PackagingModal m_packagingModal;
		ScriptWizardModal m_scriptWizardModal;
		SceneViewportPanel m_sceneViewportPanel;
		GameViewportPanel m_gameViewportPanel;

		Core::Renderer::RenderScene m_renderScene;
		std::shared_ptr<Core::RHI::IGpuTexture> m_viewportTexture;
		std::shared_ptr<Core::RHI::IGpuTexture> m_gameViewportTexture;

		// Dock visibility
		bool m_showSceneViewport = true;
		bool m_showGameViewport = true;
		bool m_showHierarchy = true;
		bool m_showInspector = true;
		bool m_showWorldSettings = true;
		bool m_showConsole = true;
		bool m_showContentBrowser = true;
		bool m_showProfiler = true;
		bool m_showStats = true;
		bool m_focusFileSystemOnOpen = true;

		CommandHistory m_commandHistory;
		SelectionContext m_selectionContext;

		float m_autosaveTimer = 0.0f;
	};
}
