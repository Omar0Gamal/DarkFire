#include "Editor/EditorLayer.h"
#include "Editor/UI/EditorIcons.h"
#include "Editor/UI/EditorTheme.h"
#include "Editor/FileDialogs.h"
#include "Editor/Clipboard.h"
#include "Editor/Commands/EntityCommands.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Core/Assets/AssetManager.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/RHI/RHIDefines.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include <algorithm>
#include <filesystem>

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

	void EditorLayer::discoverProjects()
	{
		m_projectManagerPanel.discoverProjects();
	}

	bool EditorLayer::openProject(const std::filesystem::path& projPath)
	{
		if (!std::filesystem::exists(projPath))
		{
			Core::Log::Get().error("Editor", "Project file not found: {}", projPath.string());
			return false;
		}

		auto cfgRes = Runtime::ProjectConfig::Load(projPath);
		if (cfgRes.isErr())
		{
			Core::Log::Get().error("Editor", "Failed to load project config: {}", cfgRes.error());
			return false;
		}

		m_projectConfig = cfgRes.value();
		m_currentProjectPath = projPath;
		std::filesystem::path projectDir = projPath.parent_path();

		// 1. Unload old GameModule DLL safely
		if (m_gameModule && m_gameModule->isLoaded())
		{
			if (m_activeScene)
			{
				auto view = m_activeScene->view<Core::ScriptsComponent>();
				for (auto e : view)
				{
					auto& sc = view.get<Core::ScriptsComponent>(e);
					for (auto& s : sc.scripts)
					{
						if (s.instance && s.isStarted)
						{
							s.instance->onDestroy();
							s.instance.reset();
						}
					}
				}
			}
			m_gameModule->unload();
			m_gameModule.reset();
		}

		// 2. Mount VFS
		auto& ctx = Core::EngineContext::Get();
		if (std::filesystem::exists("Engine/Content"))
		{
			ctx.vfs.mount("/engine", "Engine/Content");
		}
		else if (std::filesystem::exists("Engine"))
		{
			ctx.vfs.mount("/engine", "Engine");
		}
		ctx.vfs.mount("", projectDir);

		// 3. Initialize AssetManager & ContentBrowser
		Core::AssetManager::Get().init(projectDir);
		m_contentBrowserPanel.setRootDirectory(projectDir);

		// 4. Load Game Module DLL
		std::string configName = "Debug";
#if defined(DF_RELEASE)
		configName = "Release";
#elif defined(DF_DIST)
		configName = "Dist";
#endif

		std::string modPathStr = m_projectConfig.modulePath;
		size_t pos = modPathStr.find("%CONFIG%");
		if (pos != std::string::npos)
		{
			modPathStr.replace(pos, 8, configName);
		}

		std::filesystem::path modPath = projectDir / modPathStr;
		if (!std::filesystem::exists(modPath) || std::filesystem::is_directory(modPath))
		{
			modPath = modPathStr;
		}
		if (!std::filesystem::exists(modPath) || std::filesystem::is_directory(modPath))
		{
			modPath = "bin/" + configName + "-windows-x86_64/Templates/" + m_projectConfig.name + "/" + m_projectConfig.name + ".dll";
		}
		if (!std::filesystem::exists(modPath) || std::filesystem::is_directory(modPath))
		{
			modPath = "bin/" + configName + "-windows-x86_64/" + m_projectConfig.name + "/" + m_projectConfig.name + ".dll";
		}

		if (std::filesystem::exists(modPath) && !std::filesystem::is_directory(modPath))
		{
			Core::Log::Get().info("Editor", "Loading project Game Module DLL: {}", modPath.string());
			auto modRes = Runtime::GameModuleLoader::Load(modPath, ctx);
			if (modRes.isOk())
			{
				m_gameModule = std::make_unique<Runtime::GameModuleLoader>(std::move(modRes.value()));
				Core::Log::Get().info("Editor", "Loaded Module '{}' successfully. Registered scripts: {}",
					m_gameModule->getInfo().name, Core::ScriptRegistry::Get().getRegisteredNames().size());
			}
			else
			{
				Core::Log::Get().warn("Editor", "Could not load Game Module: {}", modRes.error());
			}
		}

		// 5. Open StartScene
		std::filesystem::path startScenePath = projectDir / m_projectConfig.startScene;
		if (std::filesystem::exists(startScenePath))
		{
			openScene(startScenePath);
		}
		else
		{
			newScene();
		}

		m_inProjectManager = false;
		m_playSession.setMode(EditorMode::Edit);
		m_focusFileSystemOnOpen = true;
		UI::DockLayout::Get().setProjectDirectory(projectDir);
		updateWindowTitle();

		return true;
	}

	void EditorLayer::closeProject()
	{
		m_inProjectManager = true;
		m_playSession.setMode(EditorMode::ProjectManager);
		UI::DockLayout::Get().setProjectDirectory("");

		discoverProjects();
		updateWindowTitle();
	}

	void EditorLayer::newScene()
	{
		m_activeScene = std::make_shared<Core::Scene>();
		m_currentScenePath.clear();
		m_isSceneDirty = false;

		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
		m_inspectorPanel.setContext(m_activeScene);

		// Default World Environment entity with Environment & PostProcessing
		auto env = m_activeScene->createEntity("World Environment");
		env.addComponent<Core::EnvironmentComponent>();
		env.addComponent<Core::PostProcessingComponent>();

		// Default environment entities: Directional Light & Camera
		auto light = m_activeScene->createEntity("Directional Light");
		light.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 10.0f, -5.0f);
		light.getComponent<Core::TransformComponent>().setEulerDegrees(Core::Vector3(45.0f, 30.0f, 0.0f));
		auto& lightComp = light.addComponent<Core::LightComponent>();
		lightComp.type = Core::LightType::Directional;
		lightComp.intensity = 1.0f;
		lightComp.color = Core::Color(1.0f, 0.98f, 0.90f, 1.0f);

		auto cam = m_activeScene->createEntity("Main Camera");
		cam.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 3.0f, -8.0f);
		cam.getComponent<Core::TransformComponent>().setEulerDegrees(Core::Vector3(15.0f, 0.0f, 0.0f));
		auto& camComp = cam.addComponent<Core::CameraComponent>();
		camComp.isPrimary = true;
		camComp.fov = 60.0f;

		updateWindowTitle();
	}

	void EditorLayer::newSandboxScene()
	{
		newScene(); // Sets up Directional Light & Main Camera

		// Add Ground Plane with Box Collider
		auto ground = m_activeScene->createEntity("Ground Plane");
		auto& gMr = ground.addComponent<Core::MeshRendererComponent>();
		gMr.meshPath = "builtin:plane";
		gMr.materialPath = "/engine/Content/Default.mat";
		auto& gTc = ground.getComponent<Core::TransformComponent>();
		gTc.position = Core::Vector3(0.0f, 0.0f, 0.0f);
		gTc.scale = Core::Vector3(30.0f, 1.0f, 30.0f);
		auto& gCol = ground.addComponent<Core::ColliderComponent>();
		gCol.shape = Core::ColliderShape::Box;
		gCol.size = Core::Vector3(30.0f, 0.1f, 30.0f);

		// Add Physics Cube
		auto cube = m_activeScene->createEntity("Physics Cube");
		auto& cMr = cube.addComponent<Core::MeshRendererComponent>();
		cMr.meshPath = "builtin:cube";
		cMr.materialPath = "/engine/Content/Default.mat";
		auto& cTc = cube.getComponent<Core::TransformComponent>();
		cTc.position = Core::Vector3(-1.5f, 2.0f, 0.0f);
		cTc.scale = Core::Vector3(1.0f, 1.0f, 1.0f);
		auto& cRb = cube.addComponent<Core::RigidbodyComponent>();
		cRb.mass = 1.0f;
		cRb.useGravity = true;
		cRb.restitution = 0.5f;
		auto& cCol = cube.addComponent<Core::ColliderComponent>();
		cCol.shape = Core::ColliderShape::Box;
		cCol.size = Core::Vector3(1.0f, 1.0f, 1.0f);

		// Add Physics Sphere
		auto sphere = m_activeScene->createEntity("Bouncing Sphere");
		auto& sMr = sphere.addComponent<Core::MeshRendererComponent>();
		sMr.meshPath = "builtin:sphere";
		sMr.materialPath = "/engine/Content/Default.mat";
		auto& sTc = sphere.getComponent<Core::TransformComponent>();
		sTc.position = Core::Vector3(1.5f, 3.5f, 0.0f);
		sTc.scale = Core::Vector3(1.0f, 1.0f, 1.0f);
		auto& sRb = sphere.addComponent<Core::RigidbodyComponent>();
		sRb.mass = 0.8f;
		sRb.useGravity = true;
		sRb.restitution = 0.85f;
		auto& sCol = sphere.addComponent<Core::ColliderComponent>();
		sCol.shape = Core::ColliderShape::Sphere;
		sCol.radius = 0.5f;

		m_isSceneDirty = true;
		updateWindowTitle();
		Core::Log::Get().info("Editor", "Spawned interactive 3D physics sandbox scene.");
	}

	void EditorLayer::openScene()
	{
		std::string path = FileDialogs::OpenFile("DarkFire Scene (*.scene)\0*.scene\0All Files (*.*)\0*.*\0\0");
		if (!path.empty())
		{
			openScene(path);
		}
	}

	void EditorLayer::openScene(const std::filesystem::path& path)
	{
		if (m_playSession.getEditorMode() != EditorMode::Edit)
		{
			onStop();
		}

		auto newScenePtr = std::make_shared<Core::Scene>();
		Core::SceneSerializer serializer(newScenePtr.get());
		if (serializer.deserializeFromFile(path))
		{
			m_activeScene = newScenePtr;
			m_currentScenePath = path;
			m_isSceneDirty = false;
			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			m_inspectorPanel.setContext(m_activeScene);
			updateWindowTitle();
			Core::Log::Get().info("Editor", "Loaded scene: {}", path.string());
		}
		else
		{
			Core::Log::Get().error("Editor", "Failed to deserialize scene: {}", path.string());
		}
	}

	void EditorLayer::saveScene()
	{
		if (m_currentScenePath.empty())
		{
			saveSceneAs();
			return;
		}

		Core::SceneSerializer serializer(m_activeScene.get());
		if (serializer.serializeToFile(m_currentScenePath))
		{
			m_isSceneDirty = false;
			updateWindowTitle();
			Core::Log::Get().info("Editor", "Saved scene to: {}", m_currentScenePath.string());
		}
	}

	void EditorLayer::saveSceneAs()
	{
		std::string path = FileDialogs::SaveFile("DarkFire Scene (*.scene)\0*.scene\0All Files (*.*)\0*.*\0\0");
		if (!path.empty())
		{
			std::filesystem::path p = path;
			if (p.extension() != ".scene")
			{
				p += ".scene";
			}
			m_currentScenePath = p;
			saveScene();
		}
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
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F7)) onSimulate();
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

			Runtime::Application::Get().getRenderer().render(m_renderScene, view);
		}

		// Render Game Viewport / Camera View
		if (m_activeScene && m_gameViewportTexture && m_showGameViewport)
		{
			uint32_t gw = m_gameViewportPanel.getWidth();
			uint32_t gh = m_gameViewportPanel.getHeight();
			float gameAspect = (gh > 0) ? (static_cast<float>(gw) / static_cast<float>(gh)) : (16.0f / 9.0f);
			Core::Renderer::RenderScene gameRenderScene;
			Core::ExtractRenderScene(*m_activeScene, gameRenderScene, gameAspect);

			Core::Renderer::RenderView gameView{};
			gameView.width = gw;
			gameView.height = gh;
			gameView.targetTexture = m_gameViewportTexture;
			gameView.isEditorView = false;

			Runtime::Application::Get().getRenderer().render(gameRenderScene, gameView);
		}
	}

	void EditorLayer::onImGuiRender()
	{
		if (m_inProjectManager)
		{
			m_projectManagerPanel.draw();
			return;
		}

		// 1. Fixed Top Bar
		drawTopBar();

		// 2. Fixed Bottom Status Bar
		drawBottomBar();

		// 3. Central DockSpace Host via DockLayout
		UI::DockLayout::Get().beginHost(m_activeWorkspace);

		// 4. Render Docks based on Active Workspace
		if (m_activeWorkspace == Workspace::Viewport3D)
		{
			if (m_showSceneViewport)
			{
				m_sceneViewportPanel.onImGuiRender(
					m_activeScene,
					m_viewportTexture,
					m_gameViewportTexture,
					m_editorCamera,
					m_selectionContext,
					m_commandHistory,
					&m_showSceneViewport
				);
			}
			if (m_showGameViewport)
			{
				m_gameViewportPanel.onImGuiRender(m_gameViewportTexture, &m_showGameViewport);
			}
			if (m_showHierarchy) m_hierarchyPanel.onImGuiRender(&m_showHierarchy);
			if (m_showInspector) m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary(), &m_showInspector);
			if (m_showContentBrowser) m_contentBrowserPanel.onImGuiRender(&m_showContentBrowser);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
			if (m_showProfiler) m_profilerPanel.onImGuiRender(ImGui::GetIO().DeltaTime, &m_showProfiler);

			if (m_focusFileSystemOnOpen)
			{
				ImGui::SetWindowFocus(UI::PanelNames::FileSystem);
				m_focusFileSystemOnOpen = false;
			}
		}
		else if (m_activeWorkspace == Workspace::Game)
		{
			if (m_showGameViewport) m_gameViewportPanel.onImGuiRender(m_gameViewportTexture, &m_showGameViewport);
			if (m_showSceneViewport)
			{
				m_sceneViewportPanel.onImGuiRender(
					m_activeScene,
					m_viewportTexture,
					m_gameViewportTexture,
					m_editorCamera,
					m_selectionContext,
					m_commandHistory,
					&m_showSceneViewport
				);
			}
			if (m_showHierarchy) m_hierarchyPanel.onImGuiRender(&m_showHierarchy);
			if (m_showInspector) m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary(), &m_showInspector);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
		}
		else if (m_activeWorkspace == Workspace::Script)
		{
			ImGui::Begin(UI::PanelNames::ScriptEditor);
			ImGui::TextColored(ImVec4(1.0f, 0.420f, 0.208f, 1.0f), "DarkFire Script Workspace");
			ImGui::Separator();
			ImGui::Text("Active Game Module: %s", m_gameModule ? m_gameModule->getInfo().name : "None loaded");
			ImGui::Text("Registered C++ Scripts: %zu", Core::ScriptRegistry::Get().getRegisteredNames().size());
			for (const auto& sName : Core::ScriptRegistry::Get().getRegisteredNames())
			{
				ImGui::BulletText("%s", sName.c_str());
			}
			ImGui::End();

			if (m_showHierarchy) m_hierarchyPanel.onImGuiRender(&m_showHierarchy);
			if (m_showContentBrowser) m_contentBrowserPanel.onImGuiRender(&m_showContentBrowser);
			if (m_showInspector) m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary(), &m_showInspector);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
		}
		else if (m_activeWorkspace == Workspace::Profiler)
		{
			if (m_showProfiler) m_profilerPanel.onImGuiRender(ImGui::GetIO().DeltaTime, &m_showProfiler);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
		}

		UI::DockLayout::Get().endHost();
	}

	void EditorLayer::drawTopBar()
	{
		const float topBarHeight = 36.0f;
		if (ImGui::BeginViewportSideBar("##TopHeaderBar", ImGui::GetMainViewport(), ImGuiDir_Up, topBarHeight,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar))
		{
			if (ImGui::BeginMenuBar())
			{
				// 1. Menus
				if (ImGui::BeginMenu("Scene"))
				{
					if (ImGui::MenuItem("New Scene", "Ctrl+N")) newScene();
					if (ImGui::MenuItem("New Sandbox Scene", "Ctrl+Shift+N")) newSandboxScene();
					if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) openScene();
					if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene();
					if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) saveSceneAs();
					ImGui::Separator();
					if (ImGui::MenuItem("Project Manager")) closeProject();
					ImGui::Separator();
					if (ImGui::MenuItem("Exit", "Alt+F4")) Runtime::Application::Get().close();
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Edit"))
				{
					if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_commandHistory.canUndo())) m_commandHistory.undo();
					if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_commandHistory.canRedo())) m_commandHistory.redo();
					ImGui::Separator();
					if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selectionContext.hasSelection()))
					{
						auto dup = Clipboard::Duplicate(m_selectionContext.getEntities(), m_activeScene.get(), m_commandHistory);
						if (!dup.empty())
						{
							m_selectionContext.clear();
							for (auto e : dup) m_selectionContext.select(e, true);
							m_isSceneDirty = true;
							updateWindowTitle();
						}
					}
					if (ImGui::MenuItem("Delete", "Delete", false, m_selectionContext.hasSelection()))
					{
						for (auto e : m_selectionContext.getEntities())
						{
							if (e.isValid())
							{
								m_commandHistory.execute<DeleteEntityCommand>(m_activeScene.get(), e);
							}
						}
						m_selectionContext.clear();
						m_isSceneDirty = true;
						updateWindowTitle();
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("View"))
				{
					ImGui::MenuItem("Scene 3D", nullptr, &m_showSceneViewport);
					ImGui::MenuItem("Game Viewport", nullptr, &m_showGameViewport);
					ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showHierarchy);
					ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
					ImGui::MenuItem("Content Browser", nullptr, &m_showContentBrowser);
					ImGui::MenuItem("Output Log", nullptr, &m_showConsole);
					ImGui::MenuItem("Profiler", nullptr, &m_showProfiler);
					ImGui::Separator();
					if (ImGui::MenuItem("Reset Godot 4 Layout"))
					{
						UI::DockLayout::Get().requestReset(m_activeWorkspace);
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Project"))
				{
					if (ImGui::MenuItem("Build C++ Project", "F7"))
					{
						Core::Log::Get().info("Editor", "Building project module...");
					}
					if (ImGui::MenuItem("Project Settings..."))
					{
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Export / Package Game..."))
					{
						Core::Log::Get().info("Editor", "Opening package export dialog...");
					}
					ImGui::EndMenu();
				}

				// Center Workspace Switcher Tabs
				ImGui::SameLine(ImGui::GetWindowWidth() * 0.38f);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.17f, 0.6f));

				auto drawWorkspaceButton = [this](const char* label, Workspace ws) {
					bool active = (m_activeWorkspace == ws);
					if (active)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.23f, 0.28f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.278f, 0.549f, 0.749f, 1.0f));
					}
					if (ImGui::Button(label, ImVec2(0, 24.0f)))
					{
						m_activeWorkspace = ws;
					}
					if (active)
					{
						ImGui::PopStyleColor(2);
					}
					ImGui::SameLine();
				};

				drawWorkspaceButton("3D Scene", Workspace::Viewport3D);
				drawWorkspaceButton("Game", Workspace::Game);
				drawWorkspaceButton("Script", Workspace::Script);
				drawWorkspaceButton("Profiler", Workspace::Profiler);

				ImGui::PopStyleColor();
				ImGui::PopStyleVar();

				// Right Playtest Controls
				ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));

				bool isPlaying = m_playSession.isPlaying();
				bool isPaused = m_playSession.isPaused();

				// Play Button
				if (!isPlaying && !isPaused)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.25f, 0.85f));
					if (ImGui::Button("▶ Play (F5)", ImVec2(80.0f, 24.0f))) onPlay();
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 0.85f));
					if (ImGui::Button("■ Stop (Shift+F5)", ImVec2(85.0f, 24.0f))) onStop();
					ImGui::PopStyleColor();
				}

				ImGui::SameLine();
				if (ImGui::Button(isPaused ? "▶ Resume" : "❚❚ Pause (F6)", ImVec2(75.0f, 24.0f)))
				{
					onPause();
				}

				ImGui::SameLine();
				if (ImGui::Button("⏭ Step", ImVec2(55.0f, 24.0f)))
				{
					onStep();
				}

				ImGui::SameLine();
				if (ImGui::Button("Run Ext", ImVec2(55.0f, 24.0f)))
				{
					onPlayStandalone();
				}

				ImGui::PopStyleVar();
				ImGui::EndMenuBar();
			}
		}
		ImGui::End();
	}

	void EditorLayer::drawBottomBar()
	{
		const float bottomBarHeight = 26.0f;
		if (ImGui::BeginViewportSideBar("##BottomStatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, bottomBarHeight,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar))
		{
			if (ImGui::BeginMenuBar())
			{
				ImGui::TextColored(ImVec4(0.278f, 0.549f, 0.749f, 1.0f), "DarkFire 2.0");
				ImGui::SameLine();
				ImGui::TextDisabled("|");
				ImGui::SameLine();

				if (m_playSession.isPlaying())
				{
					ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[PLAYING]");
				}
				else if (m_playSession.isPaused())
				{
					ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.2f, 1.0f), "[PAUSED]");
				}
				else if (m_playSession.getEditorMode() == EditorMode::Simulate)
				{
					ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "[SIMULATING]");
				}
				else
				{
					ImGui::TextDisabled("[EDIT MODE]");
				}

				ImGui::SameLine();
				ImGui::TextDisabled("| Project: %s", m_projectConfig.name.empty() ? "None" : m_projectConfig.name.c_str());

				ImGui::SameLine();
				ImGui::TextDisabled("| Scene: %s%s",
					m_currentScenePath.empty() ? "Untitled" : m_currentScenePath.filename().string().c_str(),
					m_isSceneDirty ? " *" : "");

				// Right side stats
				float rightOffset = 220.0f;
				ImGui::SameLine(ImGui::GetWindowWidth() - rightOffset);
				float fps = ImGui::GetIO().Framerate;
				ImGui::TextDisabled("%.1f FPS (%.2f ms)", fps, 1000.0f / (fps > 0 ? fps : 60.0f));

				ImGui::EndMenuBar();
			}
		}
		ImGui::End();
	}

	void EditorLayer::onPlay()
	{
		m_playSession.onPlay(m_activeScene, [this]() {
			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			m_inspectorPanel.setContext(m_activeScene);
			updateWindowTitle();
		});
	}

	void EditorLayer::onPause()
	{
		m_playSession.onPause([this]() {
			updateWindowTitle();
		});
	}

	void EditorLayer::onStep()
	{
		m_playSession.onStep(m_activeScene);
	}

	void EditorLayer::onSimulate()
	{
		m_playSession.onSimulate(m_activeScene, [this]() {
			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			m_inspectorPanel.setContext(m_activeScene);
			updateWindowTitle();
		});
	}

	void EditorLayer::onStop()
	{
		m_playSession.onStop(m_activeScene, [this]() {
			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			m_inspectorPanel.setContext(m_activeScene);
			updateWindowTitle();
		});
	}

	void EditorLayer::onPlayStandalone()
	{
		if (m_currentProjectPath.empty()) return;

		std::string configName = "Debug";
#if defined(DF_RELEASE)
		configName = "Release";
#elif defined(DF_DIST)
		configName = "Dist";
#endif

		std::filesystem::path playerExe = "bin/" + configName + "-windows-x86_64/Player/Player.exe";
		if (!std::filesystem::exists(playerExe))
		{
			Core::Log::Get().error("Editor", "Player executable not found at: {}", playerExe.string());
			return;
		}

		std::string cmd = playerExe.string() + " \"" + m_currentProjectPath.string() + "\"";
		STARTUPINFOA si{};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};

		if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			Core::Log::Get().info("Editor", "Launched Standalone Player: {}", cmd);
		}
		else
		{
			Core::Log::Get().error("Editor", "Failed to launch Player.exe");
		}
	}

	void EditorLayer::performAutosave()
	{
		if (m_playSession.getEditorMode() != EditorMode::Edit || !m_isSceneDirty || m_currentScenePath.empty()) return;

		std::filesystem::path autosavePath = m_currentScenePath;
		autosavePath += ".autosave";

		Core::SceneSerializer serializer(m_activeScene.get());
		if (serializer.serializeToFile(autosavePath))
		{
			Core::Log::Get().info("Editor", "Autosaved scene snapshot to: {}", autosavePath.string());
		}
	}

	void EditorLayer::updateWindowTitle()
	{
		std::string title = "DarkFire Engine 2.0";
		if (!m_inProjectManager)
		{
			title += " - " + (m_projectConfig.name.empty() ? "Untitled Project" : m_projectConfig.name);
			title += " | " + (m_currentScenePath.empty() ? "Untitled.scene" : m_currentScenePath.filename().string());
			if (m_isSceneDirty) title += "*";

			if (m_playSession.isPlaying()) title += " [PLAYING]";
			else if (m_playSession.isPaused()) title += " [PAUSED]";
			else if (m_playSession.getEditorMode() == EditorMode::Simulate) title += " [SIMULATING]";
		}
		else
		{
			title += " - Project Manager";
		}

		Runtime::Application::Get().getWindow().setTitle(title);
	}
}
