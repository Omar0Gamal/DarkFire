#include "Editor/EditorLayer.h"
#include "Editor/UI/EditorTheme.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/RHI/RHIDefines.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Core/Scene/Systems/ParticleSystem.h"

#include <imgui.h>
#include <filesystem>

namespace Editor
{
	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{
		m_projectManagerPanel.setOnOpenProject([this](const std::filesystem::path& p) {
			return openProject(p);
		});
		m_projectManagerPanel.setOnPlayStandalone([this](const std::filesystem::path& p) {
			m_currentProjectPath = p;
			onPlayStandalone();
		});

		m_sceneViewportPanel.setOnOpenScene([this](const std::filesystem::path& p) {
			openScene(p);
		});
		m_sceneViewportPanel.setOnNewSandboxScene([this]() {
			newSandboxScene();
		});
		m_sceneViewportPanel.setOnSceneModified([this]() {
			m_isSceneDirty = true;
			updateWindowTitle();
		});
	}

	EditorLayer::~EditorLayer()
	{
		onDetach();
	}

	void EditorLayer::onAttach()
	{
		setupImGuiStyle();
		UI::DockLayout::Get().init(m_currentProjectPath.empty() ? "" : m_currentProjectPath.parent_path());

		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
		m_inspectorPanel.setHistory(&m_commandHistory);
		m_inspectorPanel.setContext(m_activeScene);
		m_worldSettingsPanel.setScene(m_activeScene);
		m_contentBrowserPanel.setOpenSceneCallback([this](const std::filesystem::path& p) { openScene(p); });

		discoverProjects();

		if (!m_inProjectManager && !m_currentProjectPath.empty())
		{
			openProject(m_currentProjectPath);
		}
		else
		{
			newScene();
		}

		resizeViewport(m_sceneViewportPanel.getWidth(), m_sceneViewportPanel.getHeight());
		resizeGameViewport(m_gameViewportPanel.getWidth(), m_gameViewportPanel.getHeight());
		updateWindowTitle();

		Core::Log::Get().info("Editor", "DarkFire Editor attached with Godot 4 layout and theme.");
	}

	void EditorLayer::onDetach()
	{
		if (m_gameModule && m_gameModule->isLoaded())
		{
			m_gameModule->unload();
			m_gameModule.reset();
		}

		m_viewportTexture.reset();
		m_gameViewportTexture.reset();
	}

	void EditorLayer::setupImGuiStyle()
	{
		Theme::ApplyDarkFireEmberTheme(Runtime::Application::Get().getWindow().getDpiScale());
	}

	void EditorLayer::onResize(uint32_t width, uint32_t height)
	{
		(void)width;
		(void)height;
	}

	void EditorLayer::resizeViewport(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;
		m_sceneViewportPanel.setSize(width, height);

		auto& rhi = Runtime::Application::Get().getDevice();
		Core::RHI::GpuTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.format = Core::RHI::PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = Core::RHI::BufferBindFlags::RenderTarget | Core::RHI::BufferBindFlags::ShaderResource;

		auto res = rhi.createTexture(desc);
		if (res.isOk())
		{
			m_viewportTexture = *res;
		}

		m_editorCamera.setViewportSize(width, height);
	}

	void EditorLayer::resizeGameViewport(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;
		m_gameViewportPanel.setSize(width, height);

		auto& rhi = Runtime::Application::Get().getDevice();
		Core::RHI::GpuTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.format = Core::RHI::PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = Core::RHI::BufferBindFlags::RenderTarget | Core::RHI::BufferBindFlags::ShaderResource;

		auto res = rhi.createTexture(desc);
		if (res.isOk())
		{
			m_gameViewportTexture = *res;
		}
	}

	void EditorLayer::onFixedUpdate(float fixedDeltaTime)
	{
		if (m_inProjectManager) return;
		m_playSession.onFixedUpdate(m_activeScene, fixedDeltaTime);
	}

	void EditorLayer::onUpdate(float deltaTime)
	{
		if (m_inProjectManager) return;

		// Script variable-rate updates
		m_playSession.onUpdate(m_activeScene, deltaTime);

		// Keyboard Shortcuts
		bool ctrlHeld = Platform::Input::isKeyPressed(Platform::KeyCode::LeftControl) || Platform::Input::isKeyPressed(Platform::KeyCode::RightControl);
		bool shiftHeld = Platform::Input::isKeyPressed(Platform::KeyCode::LeftShift) || Platform::Input::isKeyPressed(Platform::KeyCode::RightShift);
		bool textInputActive = ImGui::GetIO().WantTextInput;

		if (!textInputActive)
		{
			if (ctrlHeld && Platform::Input::isKeyJustPressed(Platform::KeyCode::S))
			{
				if (shiftHeld) saveSceneAs();
				else saveScene();
			}
			if (ctrlHeld && Platform::Input::isKeyJustPressed(Platform::KeyCode::O)) openScene();
			if (ctrlHeld && Platform::Input::isKeyJustPressed(Platform::KeyCode::N)) newScene();
			if (ctrlHeld && Platform::Input::isKeyJustPressed(Platform::KeyCode::Z))
			{
				if (shiftHeld) m_commandHistory.redo();
				else m_commandHistory.undo();
			}

			// Gizmo toggles: Q: None, W: Translate, E: Rotate, R: Scale
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Q)) m_sceneViewportPanel.setGizmoType(-1);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::W)) m_sceneViewportPanel.setGizmoType(0);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::E)) m_sceneViewportPanel.setGizmoType(1);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::R)) m_sceneViewportPanel.setGizmoType(2);

			// Playtest F-keys
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F5))
			{
				if (shiftHeld) onStop();
				else onPlay();
			}
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F6)) onPause();
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F7)) buildAndReloadGameModule();
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F8)) onSimulate();
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F10)) onStep();
		}

		// Autosave
		m_autosaveTimer += deltaTime;
		if (m_autosaveTimer >= 300.0f)
		{
			m_autosaveTimer = 0.0f;
			performAutosave();
		}

		// Update Editor Camera
		m_editorCamera.onUpdate(deltaTime, m_sceneViewportPanel.isHovered(), m_sceneViewportPanel.isFocused());

		// Apply pending viewport resizes before rendering so textures are immediately rendered and never flicker
		if (m_sceneViewportPanel.getPendingWidth() > 0 && m_sceneViewportPanel.getPendingHeight() > 0)
		{
			resizeViewport(m_sceneViewportPanel.getPendingWidth(), m_sceneViewportPanel.getPendingHeight());
			m_sceneViewportPanel.clearPendingResize();
		}

		if (m_gameViewportPanel.getPendingWidth() > 0 && m_gameViewportPanel.getPendingHeight() > 0)
		{
			resizeGameViewport(m_gameViewportPanel.getPendingWidth(), m_gameViewportPanel.getPendingHeight());
			m_gameViewportPanel.clearPendingResize();
		}

		// Render 3D Scene Viewport
		if (m_activeScene && m_viewportTexture)
		{
			m_activeScene->updateTransforms();
			Core::Systems::ParticleSystem::Tick(*m_activeScene, deltaTime);
			Core::ExtractRenderScene(*m_activeScene, m_renderScene);

			// Override with Editor Camera
			m_renderScene.camera.position = m_editorCamera.getPosition();
			m_renderScene.camera.forward = m_editorCamera.getForward();
			m_renderScene.camera.nearZ = m_editorCamera.getNearZ();
			m_renderScene.camera.farZ = m_editorCamera.getFarZ();
			m_renderScene.camera.fov = Core::Math::toRadians(m_editorCamera.getFov());
			m_renderScene.camera.aspect = m_editorCamera.getAspectRatio();
			m_renderScene.camera.viewMatrix = m_editorCamera.getViewMatrix();
			m_renderScene.camera.projMatrix = m_editorCamera.getProjectionMatrix();
			m_renderScene.camera.viewProjMatrix = m_editorCamera.getViewProjectionMatrix();
			m_renderScene.camera.invViewMatrix = m_editorCamera.getViewMatrix().Invert();
			m_renderScene.camera.invProjMatrix = m_editorCamera.getProjectionMatrix().Invert();
			m_renderScene.camera.invViewProjMatrix = m_editorCamera.getInverseViewProjectionMatrix();

			Core::Renderer::RenderView view{};
			view.width = m_sceneViewportPanel.getWidth();
			view.height = m_sceneViewportPanel.getHeight();
			view.targetTexture = m_viewportTexture;
			view.isEditorView = true;
			view.overrides.overrideFXAA = true;
			view.overrides.enableFXAA = m_sceneViewportPanel.isFXAAEnabled();

			Runtime::Application::Get().getRenderer().render(m_renderScene, view);
		}

		// Render Game Viewport / Camera View
		// In Edit mode, optimize secondary viewport by reusing primary shadow maps and throttling to avoid halving framerate
		static uint32_t s_gameViewportThrottle = 0;
		bool shouldRenderGame = m_showGameViewport && (m_playSession.isPlaying() || (++s_gameViewportThrottle % 2 == 0));
		if (m_activeScene && m_gameViewportTexture && shouldRenderGame)
		{
			uint32_t gw = m_gameViewportPanel.getWidth();
			uint32_t gh = m_gameViewportPanel.getHeight();
			float gameAspect = (gh > 0) ? (static_cast<float>(gw) / static_cast<float>(gh)) : (16.0f / 9.0f);
			Core::Renderer::RenderScene gameRenderScene;
			Core::ExtractRenderScene(*m_activeScene, gameRenderScene, gameAspect);

			if (!m_playSession.isPlaying())
			{
				// Share existing shadow map cascade calculations from primary viewport
				gameRenderScene.environment.enableShadows = false;
			}

			Core::Renderer::RenderView gameView{};
			gameView.width = gw;
			gameView.height = gh;
			gameView.targetTexture = m_gameViewportTexture;
			gameView.isEditorView = false;

			Runtime::Application::Get().getRenderer().render(gameRenderScene, gameView);
		}
	}
}
