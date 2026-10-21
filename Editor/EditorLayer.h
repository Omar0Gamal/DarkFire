#pragma once

#include "Engine/Runtime/Layer.h"
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
#include <memory>
#include <filesystem>

namespace Editor
{
	enum class EditorMode
	{
		Edit = 0,
		Play,
		Simulate
	};

	enum class PlayState
	{
		Stopped = 0,
		Playing,
		Paused
	};

	class EditorLayer : public Runtime::Layer
	{
	public:
		EditorLayer();
		~EditorLayer() override;

		void onAttach() override;
		void onDetach() override;
		void onUpdate(float deltaTime) override;
		void onImGuiRender() override;
		void onResize(uint32_t width, uint32_t height) override;

		void newScene();
		void openScene();
		void openScene(const std::filesystem::path& path);
		void saveScene();
		void saveSceneAs();

		void onPlay();
		void onPause();
		void onStep();
		void onSimulate();
		void onStop();
		void onPlayStandalone();

		EditorMode getEditorMode() const noexcept { return m_editorMode; }
		PlayState getPlayState() const noexcept { return m_playState; }

	private:
		void setupImGuiStyle();
		void resizeViewport(uint32_t width, uint32_t height);
		void resizeGameViewport(uint32_t width, uint32_t height);
		void drawMenuBar();
		void drawToolbar();
		void drawSceneViewport();
		void drawGameViewport();
		void drawGizmo();
		void onViewportClick();

		std::shared_ptr<Core::Scene> m_activeScene;
		std::shared_ptr<Core::Scene> m_editScene;
		std::filesystem::path m_currentScenePath;

		EditorMode m_editorMode = EditorMode::Edit;
		PlayState m_playState = PlayState::Stopped;

		EditorCamera m_editorCamera;
		HierarchyPanel m_hierarchyPanel;
		InspectorPanel m_inspectorPanel;
		ConsolePanel m_consolePanel;
		ContentBrowserPanel m_contentBrowserPanel;
		ProfilerPanel m_profilerPanel;

		Core::Renderer::RenderScene m_renderScene;
		std::shared_ptr<Core::RHI::IGpuTexture> m_viewportTexture;
		std::shared_ptr<Core::RHI::IGpuTexture> m_gameViewportTexture;

		uint32_t m_viewportWidth = 1280;
		uint32_t m_viewportHeight = 720;
		bool m_viewportHovered = false;
		bool m_viewportFocused = false;
		Core::Vector2 m_viewportBounds[2]{};

		uint32_t m_gameViewportWidth = 1280;
		uint32_t m_gameViewportHeight = 720;
		int m_aspectPresetIndex = 1; // 0: Free, 1: 16:9, 2: 16:10, 3: 4:3

		int m_gizmoType = 0; // 0: Translate, 1: Rotate, 2: Scale, -1: None
		int m_gizmoMode = 1; // 0: Local, 1: World
		bool m_snapEnabled = false;
		float m_snapTranslation = 1.0f;
		float m_snapRotation = 15.0f;
		float m_snapScale = 0.5f;

		bool m_showHierarchy = true;
		bool m_showInspector = true;
		bool m_showConsole = true;
		bool m_showContentBrowser = true;
		bool m_showProfiler = true;
		bool m_showStats = true;
		bool m_showGameViewport = true;

		bool m_isSceneDirty = false;

		CommandHistory m_commandHistory;
		SelectionContext m_selectionContext;

		Core::TransformComponent m_gizmoInitialTransform{};
		bool m_gizmoWasUsing = false;

		float m_autosaveTimer = 0.0f;
		void performAutosave();
		void updateWindowTitle();
	};
}
