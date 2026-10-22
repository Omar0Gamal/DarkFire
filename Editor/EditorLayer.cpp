#include "Editor/EditorLayer.h"
#include "Editor/EditorIcons.h"
#include "Editor/EditorTheme.h"
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
#include <ImGuizmo.h>
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
		m_contentBrowserPanel.setOpenSceneCallback([this](const std::filesystem::path& p) { openScene(p); });

		std::filesystem::path defPath = FileDialogs::GetDefaultProjectsDirectory() / m_newProjectName;
		strncpy_s(m_newProjectPath, defPath.string().c_str(), sizeof(m_newProjectPath) - 1);

		discoverProjects();

		if (!m_inProjectManager && !m_currentProjectPath.empty())
		{
			openProject(m_currentProjectPath);
		}
		else
		{
			newScene();
		}

		resizeViewport(m_viewportWidth, m_viewportHeight);
		resizeGameViewport(m_gameViewportWidth, m_gameViewportHeight);
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
		m_discoveredProjects.clear();

		auto scanDir = [this](const std::filesystem::path& root) {
			if (!std::filesystem::exists(root)) return;
			std::error_code ec;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".dfproj")
				{
					ProjectItem item;
					item.path = entry.path();
					item.dir = entry.path().parent_path();
					item.name = item.dir.filename().string();
					item.lastModified = "Recent";

					auto cfgRes = Runtime::ProjectConfig::Load(item.path);
					if (cfgRes.isOk())
					{
						item.name = cfgRes.value().name;
					}

					// Avoid duplicates
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
		}
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

		if (std::filesystem::exists(modPath) && !std::filesystem::is_directory(modPath) && modPath.extension() == ".dll")
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
		m_editorMode = EditorMode::Edit;
		m_focusFileSystemOnOpen = true;
		UI::DockLayout::Get().setProjectDirectory(projectDir);
		updateWindowTitle();

		return true;
	}

	void EditorLayer::closeProject()
	{
		m_inProjectManager = true;
		m_editorMode = EditorMode::ProjectManager;
		UI::DockLayout::Get().setProjectDirectory("");
		std::filesystem::path defPath = FileDialogs::GetDefaultProjectsDirectory() / m_newProjectName;
		strncpy_s(m_newProjectPath, defPath.string().c_str(), sizeof(m_newProjectPath) - 1);

		discoverProjects();
		updateWindowTitle();
	}

	void EditorLayer::newScene()
	{
		m_activeScene = std::make_shared<Core::Scene>();
		m_currentScenePath.clear();
		m_isSceneDirty = false;

		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);

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
		auto& cRb = cube.addComponent<Core::RigidbodyComponent>();
		cRb.mass = 1.0f;
		cRb.useGravity = true;
		cRb.restitution = 0.5f;
		auto& cCol = cube.addComponent<Core::ColliderComponent>();
		cCol.shape = Core::ColliderShape::Box;
		cCol.size = Core::Vector3(1.0f, 1.0f, 1.0f);

		// Add Physics Sphere
		auto sphere = m_activeScene->createEntity("Physics Sphere");
		auto& sMr = sphere.addComponent<Core::MeshRendererComponent>();
		sMr.meshPath = "builtin:sphere";
		sMr.materialPath = "/engine/Content/Default.mat";
		auto& sTc = sphere.getComponent<Core::TransformComponent>();
		sTc.position = Core::Vector3(1.5f, 4.0f, 0.0f);
		auto& sRb = sphere.addComponent<Core::RigidbodyComponent>();
		sRb.mass = 1.0f;
		sRb.useGravity = true;
		sRb.restitution = 0.7f;
		auto& sCol = sphere.addComponent<Core::ColliderComponent>();
		sCol.shape = Core::ColliderShape::Sphere;
		sCol.radius = 0.5f;

		m_isSceneDirty = true;
		m_editorCamera.focusOn(Core::Vector3(0.0f, 1.5f, 0.0f), 8.0f);
		updateWindowTitle();
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
		if (!std::filesystem::exists(path))
		{
			Core::Log::Get().error("Editor", "Scene file does not exist: {}", path.string());
			return;
		}

		auto newSc = std::make_shared<Core::Scene>();
		Core::SceneSerializer serializer(newSc.get());
		if (serializer.deserializeFromFile(path))
		{
			m_activeScene = newSc;
			m_currentScenePath = path;
			m_isSceneDirty = false;
			m_selectionContext.clear();
			m_commandHistory.clear();

			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			updateWindowTitle();
			Core::Log::Get().info("Editor", "Successfully opened scene: {}", path.string());
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
		m_viewportWidth = width;
		m_viewportHeight = height;

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
		m_gameViewportWidth = width;
		m_gameViewportHeight = height;

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

		// Fixed physics & script simulation tick
		if ((m_editorMode == EditorMode::Play || m_editorMode == EditorMode::Simulate) && m_playState == PlayState::Playing)
		{
			if (m_activeScene)
			{
				Runtime::Application::Get().getPhysicsWorld().step(*m_activeScene, fixedDeltaTime);

				if (m_editorMode == EditorMode::Play)
				{
					auto view = m_activeScene->view<Core::ScriptsComponent>();
					for (auto handle : view)
					{
						auto& sc = view.get<Core::ScriptsComponent>(handle);
						for (auto& s : sc.scripts)
						{
							if (s.isEnabled && s.isStarted && s.instance)
							{
								s.instance->onFixedUpdate(fixedDeltaTime);
							}
						}
					}
				}
			}
		}
	}

	void EditorLayer::onUpdate(float deltaTime)
	{
		if (m_inProjectManager) return;

		// Script variable-rate updates
		if (m_editorMode == EditorMode::Play && m_playState == PlayState::Playing && m_activeScene)
		{
			auto view = m_activeScene->view<Core::ScriptsComponent>();
			for (auto handle : view)
			{
				auto& sc = view.get<Core::ScriptsComponent>(handle);
				for (auto& s : sc.scripts)
				{
					if (s.isEnabled && s.isStarted && s.instance)
					{
						s.instance->onUpdate(deltaTime);
						s.instance->onLateUpdate(deltaTime);
					}
				}
			}
		}

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
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Q)) m_gizmoType = -1;
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::W)) m_gizmoType = 0;
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::E)) m_gizmoType = 1;
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::R)) m_gizmoType = 2;

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
		m_editorCamera.onUpdate(deltaTime, m_viewportHovered, m_viewportFocused);

		// Apply pending viewport resizes before rendering so textures are immediately rendered and never flicker
		if (m_pendingViewportWidth > 0 && m_pendingViewportHeight > 0)
		{
			resizeViewport(m_pendingViewportWidth, m_pendingViewportHeight);
			m_pendingViewportWidth = 0;
			m_pendingViewportHeight = 0;
		}

		if (m_pendingGameViewportWidth > 0 && m_pendingGameViewportHeight > 0)
		{
			resizeGameViewport(m_pendingGameViewportWidth, m_pendingGameViewportHeight);
			m_pendingGameViewportWidth = 0;
			m_pendingGameViewportHeight = 0;
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
			view.width = m_viewportWidth;
			view.height = m_viewportHeight;
			view.targetTexture = m_viewportTexture;
			view.isEditorView = true;

			Runtime::Application::Get().getRenderer().render(m_renderScene, view);
		}

		// Render Game Viewport / Camera View
		if (m_activeScene && m_gameViewportTexture && m_showGameViewport)
		{
			float gameAspect = (m_gameViewportHeight > 0) ? (static_cast<float>(m_gameViewportWidth) / static_cast<float>(m_gameViewportHeight)) : (16.0f / 9.0f);
			Core::Renderer::RenderScene gameRenderScene;
			Core::ExtractRenderScene(*m_activeScene, gameRenderScene, gameAspect);

			Core::Renderer::RenderView gameView{};
			gameView.width = m_gameViewportWidth;
			gameView.height = m_gameViewportHeight;
			gameView.targetTexture = m_gameViewportTexture;
			gameView.isEditorView = false;

			Runtime::Application::Get().getRenderer().render(gameRenderScene, gameView);
		}
	}

	void EditorLayer::onImGuiRender()
	{
		if (m_inProjectManager)
		{
			drawProjectManager();
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
			if (m_showSceneViewport) drawSceneViewport(&m_showSceneViewport);
			if (m_showGameViewport) drawGameViewport(&m_showGameViewport);
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
			if (m_showGameViewport) drawGameViewport(&m_showGameViewport);
			if (m_showSceneViewport) drawSceneViewport(&m_showSceneViewport);
			if (m_showHierarchy) m_hierarchyPanel.onImGuiRender(&m_showHierarchy);
			if (m_showInspector) m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary(), &m_showInspector);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
		}
		else if (m_activeWorkspace == Workspace::Script)
		{
			// Script workspace: Central script viewer + FileSystem left + Inspector right
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
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Project"))
				{
					if (ImGui::MenuItem("Project Settings...")) {}
					if (ImGui::MenuItem("Open Project Folder"))
					{
						if (!m_currentProjectPath.empty())
						{
							ShellExecuteA(nullptr, "explore", m_currentProjectPath.parent_path().string().c_str(), nullptr, nullptr, SW_SHOW);
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Reload Game Module (Scripts)"))
					{
						if (!m_currentProjectPath.empty()) openProject(m_currentProjectPath);
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Window"))
				{
					if (ImGui::BeginMenu("Panels"))
					{
						ImGui::MenuItem("Scene", nullptr, &m_showHierarchy);
						ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
						ImGui::MenuItem("FileSystem", nullptr, &m_showContentBrowser);
						ImGui::MenuItem("Console", nullptr, &m_showConsole);
						ImGui::MenuItem("Profiler", nullptr, &m_showProfiler);
						ImGui::MenuItem("3D Viewport", nullptr, &m_showSceneViewport);
						ImGui::MenuItem("Game Viewport", nullptr, &m_showGameViewport);
						ImGui::EndMenu();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Reset Layout (Active Workspace)"))
					{
						UI::DockLayout::Get().requestReset(m_activeWorkspace);
					}
					if (ImGui::MenuItem("Reset All Layouts"))
					{
						UI::DockLayout::Get().requestResetAll();
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Editor"))
				{
					ImGui::MenuItem("Show 3D Grid", nullptr, &m_showGrid);
					ImGui::MenuItem("Show Camera PiP Preview", nullptr, &m_showPiP);
					ImGui::Separator();
					auto rConfig = Runtime::Application::Get().getRenderer().getConfig();
					bool fxaaEnabled = rConfig.enableFXAA;
					if (ImGui::MenuItem("Anti-Aliasing (FXAA)", nullptr, &fxaaEnabled))
					{
						rConfig.enableFXAA = fxaaEnabled;
						Runtime::Application::Get().getRenderer().setConfig(rConfig);
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Help"))
				{
					if (ImGui::MenuItem("Documentation")) ShellExecuteA(nullptr, "open", "https://github.com", nullptr, nullptr, SW_SHOW);
					if (ImGui::MenuItem("About DarkFire Engine")) {}
					ImGui::EndMenu();
				}

				// Project Badge
				ImGui::SameLine();
				std::string badge = "[ " + (m_projectConfig.name.empty() ? std::string("Playground3D") : m_projectConfig.name) + " ]";
				if (ImGui::SmallButton(badge.c_str()))
				{
					closeProject();
				}

				// 2. Center Workspace Switcher
				float wsWidth = 240.0f;
				ImGui::SetCursorPosX((ImGui::GetWindowWidth() - wsWidth) * 0.48f);

				auto wsButton = [this](const char* label, Workspace ws) {
					bool active = (m_activeWorkspace == ws);
					if (active)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.000f, 0.420f, 0.208f, 0.85f));
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
					}
					else
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.15f, 0.18f, 0.0f));
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.75f, 1.0f));
					}
					if (ImGui::Button(label, ImVec2(55.0f, 24.0f)))
					{
						m_activeWorkspace = ws;
					}
					ImGui::PopStyleColor(2);
					ImGui::SameLine();
				};

				wsButton("3D", Workspace::Viewport3D);
				wsButton("Game", Workspace::Game);
				wsButton("Script", Workspace::Script);
				wsButton("Profiler", Workspace::Profiler);

				// 3. Right Playtest Controls
				ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 320.0f);

				bool isPlaying = (m_playState == PlayState::Playing || m_playState == PlayState::Paused);

				// Play / Stop
				if (m_editorMode == EditorMode::Edit)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.4f, 1.0f));
					if (ImGui::Button("[ ▶ Play ]", ImVec2(70, 24))) onPlay();
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
					if (ImGui::Button("[ ⏹ Stop ]", ImVec2(70, 24))) onStop();
					ImGui::PopStyleColor(2);
				}
				ImGui::SameLine();

				// Pause
				if (isPlaying)
				{
					const char* pText = (m_playState == PlayState::Paused) ? "Resume" : "Pause";
					if (ImGui::Button(pText, ImVec2(55, 24))) onPause();
					ImGui::SameLine();

					if (m_playState == PlayState::Paused)
					{
						if (ImGui::Button("Step", ImVec2(45, 24))) onStep();
						ImGui::SameLine();
					}
				}
				else
				{
					if (ImGui::Button("Simulate", ImVec2(65, 24))) onSimulate();
					ImGui::SameLine();
				}

				// Standalone
				if (ImGui::Button("Run", ImVec2(45, 24))) onPlayStandalone();
				ImGui::SameLine();

				// D3D11 Pill
				ImGui::TextDisabled("D3D11-RevZ");

				ImGui::EndMenuBar();
			}
			ImGui::End();
		}
	}

	void EditorLayer::drawBottomBar()
	{
		const float bottomBarHeight = 26.0f;
		if (ImGui::BeginViewportSideBar("##BottomStatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, bottomBarHeight,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar))
		{
			if (ImGui::BeginMenuBar())
			{
				// Left drawer buttons
				if (ImGui::SmallButton("Output (0)")) m_showConsole = !m_showConsole;
				ImGui::SameLine();
				if (ImGui::SmallButton("Profiler")) m_showProfiler = !m_showProfiler;
				ImGui::SameLine();
				if (ImGui::SmallButton("FileSystem")) m_showContentBrowser = !m_showContentBrowser;

				// Center: Scene Status
				ImGui::SameLine(ImGui::GetWindowWidth() * 0.45f);
				if (m_isSceneDirty)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "* Modified");
				}
				else
				{
					ImGui::TextDisabled("Saved");
				}

				// Right: Telemetry
				const auto& r = Runtime::Application::Get().getRenderer();
				float dpi = Runtime::Application::Get().getWindow().getDpiScale();
				ImGui::SameLine(ImGui::GetWindowWidth() - 280.0f);
				ImGui::Text("%.1f FPS (%.1fms) | DrawCalls: %u | DPI: %.1fx",
					ImGui::GetIO().Framerate,
					1000.0f / ImGui::GetIO().Framerate,
					r.getDrawCallCount(),
					dpi);

				ImGui::EndMenuBar();
			}
			ImGui::End();
		}
	}

	void EditorLayer::drawProjectManager()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
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
			if (!p.empty())
			{
				openProject(p);
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
				openProject(proj.path);
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
				openProject(selProj.path);
			}
			ImGui::PopStyleColor(3);

			ImGui::Spacing();
			if (ImGui::Button("Run Standalone", ImVec2(-1, 28.0f)))
			{
				m_currentProjectPath = selProj.path;
				onPlayStandalone();
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
						std::string tmplName = (m_newProjectTemplate == 0) ? "Playground3D" : "Empty3D";
						f << "{\n"
						  << "  \"project_version\": 2,\n"
						  << "  \"name\": \"" << m_newProjectName << "\",\n"
						  << "  \"start_scene\": \"Scenes/Start.scene\",\n"
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
				openProject(dfproj);
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0)))
			{
				m_showNewProjectModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	void EditorLayer::drawSceneViewport(bool* p_open)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin(UI::PanelNames::Viewport3D, p_open);

		// Viewport Sub-header / In-viewport toolbar overlay
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		m_viewportBounds[0] = { cursorScreenPos.x, cursorScreenPos.y };
		m_viewportBounds[1] = { cursorScreenPos.x + viewportSize.x, cursorScreenPos.y + viewportSize.y };

		m_viewportFocused = ImGui::IsWindowFocused();
		m_viewportHovered = ImGui::IsWindowHovered();

		uint32_t newW = static_cast<uint32_t>(viewportSize.x);
		uint32_t newH = static_cast<uint32_t>(viewportSize.y);

		if (newW > 0 && newH > 0 && (newW != m_viewportWidth || newH != m_viewportHeight))
		{
			m_pendingViewportWidth = newW;
			m_pendingViewportHeight = newH;
		}

		if (m_viewportTexture && m_viewportTexture->getShaderResourceView())
		{
			ImGui::Image(
				(ImTextureID)m_viewportTexture->getShaderResourceView(),
				viewportSize
			);
		}

		// 1. Draw 3D Ground Grid
		if (m_showGrid)
		{
			drawGrid3D(m_editorCamera.getViewProjectionMatrix());
		}

		// 2. Draw Viewport Icons (Camera, Lights)
		drawViewportIcons(m_editorCamera.getViewProjectionMatrix());

		// 3. Draw Selection Wireframe
		drawSelectionWireframes(m_editorCamera.getViewProjectionMatrix());

		// 4. Draw ImGuizmo
		drawGizmo();

		// 5. In-Viewport Toolbar Overlay (top-left)
		ImGui::SetCursorPos(ImVec2(12.0f, 28.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.17f, 0.75f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

		if (ImGui::Button(m_gizmoType == 0 ? "[W] Move*" : "[W] Move")) m_gizmoType = 0;
		ImGui::SameLine();
		if (ImGui::Button(m_gizmoType == 1 ? "[E] Rotate*" : "[E] Rotate")) m_gizmoType = 1;
		ImGui::SameLine();
		if (ImGui::Button(m_gizmoType == 2 ? "[R] Scale*" : "[R] Scale")) m_gizmoType = 2;
		ImGui::SameLine();
		if (ImGui::Button(m_gizmoType == -1 ? "[Q] Select*" : "[Q] Select")) m_gizmoType = -1;

		ImGui::SameLine();
		if (ImGui::Button(m_gizmoMode == 1 ? "World" : "Local")) m_gizmoMode = 1 - m_gizmoMode;

		ImGui::SameLine();
		if (ImGui::Button(m_snapEnabled ? "Snap On" : "Snap Off")) m_snapEnabled = !m_snapEnabled;

		ImGui::SameLine();
		if (ImGui::Button(m_showWireframeMode ? "Wireframe" : "Shaded")) m_showWireframeMode = !m_showWireframeMode;

		ImGui::SameLine();
		auto rendererConfig = Runtime::Application::Get().getRenderer().getConfig();
		if (ImGui::Button(rendererConfig.enableFXAA ? "FXAA" : "No AA"))
		{
			rendererConfig.enableFXAA = !rendererConfig.enableFXAA;
			Runtime::Application::Get().getRenderer().setConfig(rendererConfig);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Toggle FXAA (Fast Approximate Anti-Aliasing)");
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.35f, 0.85f));
		if (ImGui::Button("+ Sandbox"))
		{
			newSandboxScene();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Populate scene with sun, camera, ground plane, and physics shapes");
		}
		ImGui::PopStyleColor();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor();

		// 6. Orientation Gizmo (top-right)
		drawOrientationGizmo(m_viewportBounds[0], Core::Vector2(viewportSize.x, viewportSize.y));

		// 7. Camera PiP Preview (bottom-right) - only when a Camera entity is selected
		if (m_showPiP && m_selectionContext.hasSelection())
		{
			Core::Entity selectedEntity = m_selectionContext.getPrimary();
			if (selectedEntity.isValid() && selectedEntity.hasComponent<Core::CameraComponent>())
			{
				drawCameraPiP(m_viewportBounds[0], Core::Vector2(viewportSize.x, viewportSize.y));
			}
		}

		// Asset Drag & Drop into 3D Viewport
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DARKFIRE_ASSET_PATH"))
			{
				const char* pathStr = static_cast<const char*>(payload->Data);
				std::filesystem::path assetPath = pathStr;
				if (assetPath.extension() == ".scene")
				{
					openScene(assetPath);
				}
				else if (assetPath.extension() == ".obj")
				{
					if (m_activeScene)
					{
						auto ent = m_activeScene->createEntity(assetPath.stem().string());
						auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
						mesh.meshPath = assetPath.generic_string();
						mesh.materialPath = "/engine/Content/Default.mat";
						m_commandHistory.execute<CreateEntityCommand>(m_activeScene.get(), ent);
						m_selectionContext.select(ent, false);
						m_isSceneDirty = true;
						updateWindowTitle();
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	static bool ProjectClippedSegment(
		const Core::Vector3& p0, const Core::Vector3& p1,
		const Core::Matrix& viewProj,
		float startX, float startY, float vpW, float vpH,
		ImVec2& out0, ImVec2& out1)
	{
		Core::Vector4 c0 = Core::Vector4::Transform(Core::Vector4(p0.x, p0.y, p0.z, 1.0f), viewProj);
		Core::Vector4 c1 = Core::Vector4::Transform(Core::Vector4(p1.x, p1.y, p1.z, 1.0f), viewProj);

		const float nearW = 0.05f;
		const float farW  = 1000.0f;

		// Discard if completely outside near or far plane
		if ((c0.w < nearW && c1.w < nearW) || (c0.w > farW && c1.w > farW))
			return false;

		// Clip against near plane
		if (c0.w < nearW)
		{
			float t = (nearW - c0.w) / (c1.w - c0.w);
			c0 = c0 + (c1 - c0) * t;
		}
		else if (c1.w < nearW)
		{
			float t = (nearW - c1.w) / (c0.w - c1.w);
			c1 = c1 + (c0 - c1) * t;
		}

		// Clip against far plane
		if (c0.w > farW)
		{
			float t = (farW - c0.w) / (c1.w - c0.w);
			c0 = c0 + (c1 - c0) * t;
		}
		else if (c1.w > farW)
		{
			float t = (farW - c1.w) / (c0.w - c1.w);
			c1 = c1 + (c0 - c1) * t;
		}

		float ndc0X = c0.x / c0.w;
		float ndc0Y = c0.y / c0.w;
		float ndc1X = c1.x / c1.w;
		float ndc1Y = c1.y / c1.w;

		out0.x = startX + (ndc0X * 0.5f + 0.5f) * vpW;
		out0.y = startY + (0.5f - ndc0Y * 0.5f) * vpH;
		out1.x = startX + (ndc1X * 0.5f + 0.5f) * vpW;
		out1.y = startY + (0.5f - ndc1Y * 0.5f) * vpH;

		return true;
	}

	void EditorLayer::drawGrid3D(const Core::Matrix& viewProj)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float vpW = static_cast<float>(m_viewportWidth);
		float vpH = static_cast<float>(m_viewportHeight);
		float startX = m_viewportBounds[0].x;
		float startY = m_viewportBounds[0].y;

		Core::Vector3 camPos = m_editorCamera.getPosition();
		float centerSnapX = std::floor(camPos.x);
		float centerSnapZ = std::floor(camPos.z);

		const int halfLines = 150;
		const float range = static_cast<float>(halfLines);

		ImVec2 p0, p1;

		// Lines parallel to Z (varying X)
		for (int i = -halfLines; i <= halfLines; ++i)
		{
			float x = centerSnapX + static_cast<float>(i);
			float zStart = centerSnapZ - range;
			float zEnd   = centerSnapZ + range;

			if (ProjectClippedSegment(
				Core::Vector3(x, 0.0f, zStart),
				Core::Vector3(x, 0.0f, zEnd),
				viewProj, startX, startY, vpW, vpH, p0, p1))
			{
				bool isOrigin = (std::abs(x) < 0.001f);
				bool isMajor = (static_cast<int>(std::round(x)) % 10 == 0);

				float distRatio = static_cast<float>(std::abs(i)) / range;
				float fade = std::clamp((1.0f - distRatio) * (1.0f - distRatio), 0.0f, 1.0f);

				ImU32 col;
				float thickness = 1.0f;
				if (isOrigin)
				{
					col = IM_COL32(60, 140, 240, static_cast<int>(220.0f * fade)); // Blue for Z-axis
					thickness = 2.0f;
				}
				else if (isMajor)
				{
					col = IM_COL32(110, 115, 135, static_cast<int>(130.0f * fade));
					thickness = 1.2f;
				}
				else
				{
					col = IM_COL32(75, 80, 95, static_cast<int>(60.0f * fade));
				}

				if (fade > 0.01f)
				{
					drawList->AddLine(p0, p1, col, thickness);
				}
			}
		}

		// Lines parallel to X (varying Z)
		for (int i = -halfLines; i <= halfLines; ++i)
		{
			float z = centerSnapZ + static_cast<float>(i);
			float xStart = centerSnapX - range;
			float xEnd   = centerSnapX + range;

			if (ProjectClippedSegment(
				Core::Vector3(xStart, 0.0f, z),
				Core::Vector3(xEnd, 0.0f, z),
				viewProj, startX, startY, vpW, vpH, p0, p1))
			{
				bool isOrigin = (std::abs(z) < 0.001f);
				bool isMajor = (static_cast<int>(std::round(z)) % 10 == 0);

				float distRatio = static_cast<float>(std::abs(i)) / range;
				float fade = std::clamp((1.0f - distRatio) * (1.0f - distRatio), 0.0f, 1.0f);

				ImU32 col;
				float thickness = 1.0f;
				if (isOrigin)
				{
					col = IM_COL32(240, 70, 70, static_cast<int>(220.0f * fade)); // Red for X-axis
					thickness = 2.0f;
				}
				else if (isMajor)
				{
					col = IM_COL32(110, 115, 135, static_cast<int>(130.0f * fade));
					thickness = 1.2f;
				}
				else
				{
					col = IM_COL32(75, 80, 95, static_cast<int>(60.0f * fade));
				}

				if (fade > 0.01f)
				{
					drawList->AddLine(p0, p1, col, thickness);
				}
			}
		}
	}

	void EditorLayer::drawViewportIcons(const Core::Matrix& viewProj)
	{
		if (!m_activeScene) return;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float vpW = static_cast<float>(m_viewportWidth);
		float vpH = static_cast<float>(m_viewportHeight);
		float startX = m_viewportBounds[0].x;
		float startY = m_viewportBounds[0].y;

		ImVec2 mousePos = ImGui::GetMousePos();
		bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
			m_viewportHovered &&
			!ImGuizmo::IsOver() &&
			!ImGuizmo::IsUsing();

		auto projectPoint = [&](const Core::Vector3& v, ImVec2& outPt) -> bool {
			Core::Vector4 pCS = Core::Vector4::Transform(Core::Vector4(v.x, v.y, v.z, 1.0f), viewProj);
			if (pCS.w < 0.05f) return false;
			float ndcX = pCS.x / pCS.w;
			float ndcY = pCS.y / pCS.w;
			outPt.x = startX + (ndcX * 0.5f + 0.5f) * vpW;
			outPt.y = startY + (0.5f - ndcY * 0.5f) * vpH;
			return true;
		};

		// 1. Directional & Point Lights
		auto lightView = m_activeScene->view<Core::LightComponent, Core::TransformComponent>();
		for (auto handle : lightView)
		{
			Core::Entity entity{ handle, m_activeScene.get() };
			const auto& tc = lightView.get<Core::TransformComponent>(handle);
			const auto& light = lightView.get<Core::LightComponent>(handle);

			bool isSelected = m_selectionContext.isSelected(entity);
			ImVec2 screenPos;
			if (!projectPoint(tc.position, screenPos)) continue;

			if (screenPos.x < startX || screenPos.x > startX + vpW ||
				screenPos.y < startY || screenPos.y > startY + vpH)
				continue;

			if (light.type == Core::LightType::Directional)
			{
				float radius = 10.0f;
				ImU32 sunCoreCol = isSelected ? IM_COL32(255, 240, 100, 255) : IM_COL32(255, 205, 50, 220);
				ImU32 sunRayCol  = isSelected ? IM_COL32(255, 255, 180, 255) : IM_COL32(255, 180, 30, 180);

				drawList->AddCircleFilled(screenPos, radius, sunCoreCol, 16);
				drawList->AddCircle(screenPos, radius + 1.5f, IM_COL32(40, 30, 0, 150), 16, 1.0f);

				for (int r = 0; r < 8; ++r)
				{
					float angle = r * (3.14159265f / 4.0f);
					ImVec2 rStart(screenPos.x + std::cos(angle) * (radius + 2.0f), screenPos.y + std::sin(angle) * (radius + 2.0f));
					ImVec2 rEnd(screenPos.x + std::cos(angle) * (radius + 6.0f), screenPos.y + std::sin(angle) * (radius + 6.0f));
					drawList->AddLine(rStart, rEnd, sunRayCol, 1.5f);
				}

				Core::Vector3 fwd = tc.getForward();
				ImVec2 arr0, arr1;
				if (ProjectClippedSegment(tc.position, tc.position + fwd * 2.0f, viewProj, startX, startY, vpW, vpH, arr0, arr1))
				{
					drawList->AddLine(arr0, arr1, IM_COL32(255, 220, 80, 220), 2.0f);
					drawList->AddCircleFilled(arr1, 3.5f, IM_COL32(255, 230, 120, 240));
				}

				float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
				if (mouseClicked && distSq < 16.0f * 16.0f)
				{
					m_selectionContext.select(entity, false);
				}
			}
			else if (light.type == Core::LightType::Point)
			{
				float radius = 9.0f;
				ImU32 bulbCol = isSelected ? IM_COL32(255, 240, 140, 255) : IM_COL32(240, 180, 70, 220);
				drawList->AddCircleFilled(screenPos, radius, bulbCol, 16);
				drawList->AddCircle(screenPos, radius + 1.0f, IM_COL32(50, 40, 10, 150), 16, 1.0f);

				drawList->AddLine(ImVec2(screenPos.x - 3, screenPos.y - 2), ImVec2(screenPos.x + 3, screenPos.y - 2), IM_COL32(255, 255, 255, 200), 1.0f);

				if (isSelected && light.range > 0.1f)
				{
					const int segs = 32;
					for (int s = 0; s < segs; ++s)
					{
						float a0 = s * (6.2831853f / segs);
						float a1 = (s + 1) * (6.2831853f / segs);
						Core::Vector3 cp0 = tc.position + Core::Vector3(std::cos(a0) * light.range, 0.0f, std::sin(a0) * light.range);
						Core::Vector3 cp1 = tc.position + Core::Vector3(std::cos(a1) * light.range, 0.0f, std::sin(a1) * light.range);
						ImVec2 sp0, sp1;
						if (ProjectClippedSegment(cp0, cp1, viewProj, startX, startY, vpW, vpH, sp0, sp1))
						{
							drawList->AddLine(sp0, sp1, IM_COL32(255, 210, 70, 140), 1.2f);
						}
					}
				}

				float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
				if (mouseClicked && distSq < 14.0f * 14.0f)
				{
					m_selectionContext.select(entity, false);
				}
			}
		}

		// 2. Cameras
		auto camView = m_activeScene->view<Core::CameraComponent, Core::TransformComponent>();
		for (auto handle : camView)
		{
			Core::Entity entity{ handle, m_activeScene.get() };
			const auto& tc = camView.get<Core::TransformComponent>(handle);
			const auto& cam = camView.get<Core::CameraComponent>(handle);

			bool isSelected = m_selectionContext.isSelected(entity);
			ImVec2 screenPos;
			if (!projectPoint(tc.position, screenPos)) continue;

			if (screenPos.x >= startX && screenPos.x <= startX + vpW &&
				screenPos.y >= startY && screenPos.y <= startY + vpH)
			{
				ImU32 camCol = isSelected ? IM_COL32(100, 220, 255, 255) : IM_COL32(60, 160, 220, 220);
				ImVec2 bMin(screenPos.x - 9.0f, screenPos.y - 6.0f);
				ImVec2 bMax(screenPos.x + 5.0f, screenPos.y + 6.0f);
				drawList->AddRectFilled(bMin, bMax, camCol, 2.0f);
				drawList->AddRect(bMin, bMax, IM_COL32(10, 30, 50, 180), 2.0f, 0, 1.0f);

				ImVec2 lensPts[3] = {
					ImVec2(screenPos.x + 5.0f, screenPos.y - 3.5f),
					ImVec2(screenPos.x + 10.0f, screenPos.y - 6.5f),
					ImVec2(screenPos.x + 10.0f, screenPos.y + 6.5f)
				};
				drawList->AddTriangleFilled(lensPts[0], lensPts[1], lensPts[2], camCol);

				float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
				if (mouseClicked && distSq < 15.0f * 15.0f)
				{
					m_selectionContext.select(entity, false);
				}
			}

			if (isSelected)
			{
				float dist = 3.5f;
				float fovRads = Core::Math::toRadians(cam.fov);
				float h = 2.0f * dist * std::tan(fovRads * 0.5f);
				float w = h * (16.0f / 9.0f);

				Core::Vector3 fwd = tc.getForward();
				Core::Vector3 right = tc.getRight();
				Core::Vector3 up = tc.getUp();

				Core::Vector3 center = tc.position + fwd * dist;
				Core::Vector3 halfR = right * (w * 0.5f);
				Core::Vector3 halfU = up * (h * 0.5f);

				Core::Vector3 c0 = center - halfR - halfU;
				Core::Vector3 c1 = center + halfR - halfU;
				Core::Vector3 c2 = center + halfR + halfU;
				Core::Vector3 c3 = center - halfR + halfU;

				ImVec2 p0, p1;
				ImU32 frustumCol = IM_COL32(80, 200, 255, 200);

				if (ProjectClippedSegment(c0, c1, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);
				if (ProjectClippedSegment(c1, c2, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);
				if (ProjectClippedSegment(c2, c3, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);
				if (ProjectClippedSegment(c3, c0, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);

				if (ProjectClippedSegment(tc.position, c0, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
				if (ProjectClippedSegment(tc.position, c1, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
				if (ProjectClippedSegment(tc.position, c2, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
				if (ProjectClippedSegment(tc.position, c3, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);

				Core::Vector3 topMid = (c2 + c3) * 0.5f;
				Core::Vector3 topApex = topMid + up * (h * 0.25f);
				if (ProjectClippedSegment(c3, topApex, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
				if (ProjectClippedSegment(c2, topApex, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
			}
		}
	}

	void EditorLayer::drawSelectionWireframes(const Core::Matrix& viewProj)
	{
		if (!m_selectionContext.hasSelection()) return;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float vpW = static_cast<float>(m_viewportWidth);
		float vpH = static_cast<float>(m_viewportHeight);
		float startX = m_viewportBounds[0].x;
		float startY = m_viewportBounds[0].y;

		for (auto entity : m_selectionContext.getEntities())
		{
			if (!entity.isValid() || !entity.hasComponent<Core::TransformComponent>()) continue;

			const auto& tc = entity.getComponent<Core::TransformComponent>();
			Core::Vector3 p = tc.position;
			Core::Vector3 s = tc.scale * 0.5f;

			Core::Vector3 corners[8] = {
				p + Core::Vector3(-s.x, -s.y, -s.z),
				p + Core::Vector3( s.x, -s.y, -s.z),
				p + Core::Vector3( s.x,  s.y, -s.z),
				p + Core::Vector3(-s.x,  s.y, -s.z),
				p + Core::Vector3(-s.x, -s.y,  s.z),
				p + Core::Vector3( s.x, -s.y,  s.z),
				p + Core::Vector3( s.x,  s.y,  s.z),
				p + Core::Vector3(-s.x,  s.y,  s.z)
			};

			static const int edges[12][2] = {
				{0, 1}, {1, 2}, {2, 3}, {3, 0},
				{4, 5}, {5, 6}, {6, 7}, {7, 4},
				{0, 4}, {1, 5}, {2, 6}, {3, 7}
			};

			ImU32 colCoral = IM_COL32(252, 127, 127, 240); // Godot Coral Red
			for (int e = 0; e < 12; ++e)
			{
				ImVec2 ptA, ptB;
				if (ProjectClippedSegment(corners[edges[e][0]], corners[edges[e][1]], viewProj, startX, startY, vpW, vpH, ptA, ptB))
				{
					drawList->AddLine(ptA, ptB, colCoral, 1.5f);
				}
			}
		}
	}

	void EditorLayer::drawOrientationGizmo(Core::Vector2 viewportPos, Core::Vector2 viewportSize)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float radius = 32.0f;
		ImVec2 center(viewportPos.x + viewportSize.x - radius - 16.0f, viewportPos.y + radius + 16.0f);

		Core::Matrix viewMat = m_editorCamera.getViewMatrix();

		auto projectAxis = [&](const Core::Vector3& axis) -> ImVec2 {
			Core::Vector3 transformed = Core::Vector3::TransformNormal(axis, viewMat);
			return ImVec2(center.x + transformed.x * radius, center.y - transformed.y * radius);
		};

		ImVec2 ptX = projectAxis(Core::Vector3(1, 0, 0));
		ImVec2 ptY = projectAxis(Core::Vector3(0, 1, 0));
		ImVec2 ptZ = projectAxis(Core::Vector3(0, 0, 1));

		// Draw base circle
		drawList->AddCircleFilled(center, radius + 4.0f, IM_COL32(30, 34, 40, 180));
		drawList->AddCircle(center, radius + 4.0f, IM_COL32(50, 55, 65, 200), 24, 1.0f);

		// X Axis (Red)
		drawList->AddLine(center, ptX, IM_COL32(230, 60, 60, 255), 2.0f);
		drawList->AddCircleFilled(ptX, 6.0f, IM_COL32(230, 60, 60, 255));

		// Y Axis (Green)
		drawList->AddLine(center, ptY, IM_COL32(60, 210, 80, 255), 2.0f);
		drawList->AddCircleFilled(ptY, 6.0f, IM_COL32(60, 210, 80, 255));

		// Z Axis (Blue)
		drawList->AddLine(center, ptZ, IM_COL32(60, 120, 230, 255), 2.0f);
		drawList->AddCircleFilled(ptZ, 6.0f, IM_COL32(60, 120, 230, 255));
	}

	void EditorLayer::drawCameraPiP(Core::Vector2 viewportPos, Core::Vector2 viewportSize)
	{
		if (!m_gameViewportTexture || !m_gameViewportTexture->getShaderResourceView()) return;

		float pipW = 240.0f;
		float pipH = 135.0f;
		ImVec2 pipPos(viewportPos.x + viewportSize.x - pipW - 16.0f, viewportPos.y + viewportSize.y - pipH - 16.0f);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(ImVec2(pipPos.x - 2, pipPos.y - 20), ImVec2(pipPos.x + pipW + 2, pipPos.y + pipH + 2), IM_COL32(24, 28, 34, 230), 4.0f);
		drawList->AddRect(ImVec2(pipPos.x - 2, pipPos.y - 20), ImVec2(pipPos.x + pipW + 2, pipPos.y + pipH + 2), IM_COL32(71, 140, 191, 200), 4.0f, 0, 1.5f);
		drawList->AddText(ImVec2(pipPos.x + 6, pipPos.y - 18), IM_COL32(200, 210, 225, 255), "Camera Preview (PiP)");

		drawList->AddImage(
			(ImTextureID)m_gameViewportTexture->getShaderResourceView(),
			pipPos,
			ImVec2(pipPos.x + pipW, pipPos.y + pipH)
		);
	}

	void EditorLayer::drawGameViewport(bool* p_open)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin(UI::PanelNames::GameViewport, p_open);

		ImVec2 avail = ImGui::GetContentRegionAvail();
		uint32_t newW = static_cast<uint32_t>(avail.x);
		uint32_t newH = static_cast<uint32_t>(avail.y);

		if (newW > 0 && newH > 0 && (newW != m_gameViewportWidth || newH != m_gameViewportHeight))
		{
			m_pendingGameViewportWidth = newW;
			m_pendingGameViewportHeight = newH;
		}

		if (m_gameViewportTexture && m_gameViewportTexture->getShaderResourceView())
		{
			ImGui::Image(
				(ImTextureID)m_gameViewportTexture->getShaderResourceView(),
				avail
			);
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::drawGizmo()
	{
		if (m_gizmoType == -1 || !m_selectionContext.hasSelection()) return;

		Core::Entity selectedEntity = m_selectionContext.getPrimary();
		if (!selectedEntity.isValid() || !selectedEntity.hasComponent<Core::TransformComponent>()) return;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(m_viewportBounds[0].x, m_viewportBounds[0].y, static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight));

		auto viewMat = m_editorCamera.getViewMatrix();
		auto projMat = m_editorCamera.getProjectionMatrix();

		auto& tc = selectedEntity.getComponent<Core::TransformComponent>();
		Core::Matrix worldMat = tc.getLocalMatrix();

		ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
		if (m_gizmoType == 1) op = ImGuizmo::ROTATE;
		else if (m_gizmoType == 2) op = ImGuizmo::SCALE;

		ImGuizmo::MODE mode = (m_gizmoMode == 1) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

		float snapValues[3] = { m_snapTranslation, m_snapTranslation, m_snapTranslation };
		if (m_gizmoType == 1) snapValues[0] = snapValues[1] = snapValues[2] = m_snapRotation;
		else if (m_gizmoType == 2) snapValues[0] = snapValues[1] = snapValues[2] = m_snapScale;

		ImGuizmo::Manipulate(
			reinterpret_cast<const float*>(&viewMat),
			reinterpret_cast<const float*>(&projMat),
			op,
			mode,
			reinterpret_cast<float*>(&worldMat),
			nullptr,
			m_snapEnabled ? snapValues : nullptr
		);

		if (ImGuizmo::IsUsing())
		{
			if (!m_gizmoWasUsing)
			{
				m_gizmoInitialTransform = tc;
				m_gizmoWasUsing = true;
			}

			Core::Vector3 trans, scale;
			Core::Quaternion rot;
			worldMat.Decompose(scale, rot, trans);
			tc.position = trans;
			tc.rotation = rot;
			tc.scale = scale;
			m_isSceneDirty = true;
		}
		else if (m_gizmoWasUsing)
		{
			m_commandHistory.execute<ModifyTransformCommand>(
				m_activeScene.get(),
				selectedEntity.getUUID(),
				m_gizmoInitialTransform,
				tc,
				false
			);
			m_gizmoWasUsing = false;
		}
	}

	void EditorLayer::onViewportClick()
	{
	}

	void EditorLayer::onPlay()
	{
		if (m_playState == PlayState::Playing) return;

		if (m_playState == PlayState::Paused)
		{
			m_playState = PlayState::Playing;
			updateWindowTitle();
			return;
		}

		m_editScene = m_activeScene;
		m_activeScene = Core::Scene::copy(*m_editScene);

		m_editorMode = EditorMode::Play;
		m_playState = PlayState::Playing;

		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);

		// Start all attached scripts
		auto view = m_activeScene->view<Core::ScriptsComponent>();
		for (auto handle : view)
		{
			auto& sc = view.get<Core::ScriptsComponent>(handle);
			for (auto& s : sc.scripts)
			{
				if (s.isEnabled && s.instance && !s.isStarted)
				{
					s.instance->onStart();
					s.isStarted = true;
				}
			}
		}

		updateWindowTitle();
		Core::Log::Get().info("Editor", "Play mode started.");
	}

	void EditorLayer::onPause()
	{
		if (m_playState == PlayState::Playing)
		{
			m_playState = PlayState::Paused;
			updateWindowTitle();
			Core::Log::Get().info("Editor", "Play mode paused.");
		}
		else if (m_playState == PlayState::Paused)
		{
			m_playState = PlayState::Playing;
			updateWindowTitle();
			Core::Log::Get().info("Editor", "Play mode resumed.");
		}
	}

	void EditorLayer::onStep()
	{
		if (m_playState == PlayState::Paused && m_activeScene)
		{
			float dt = 1.0f / 60.0f;
			Runtime::Application::Get().getPhysicsWorld().step(*m_activeScene, dt);

			auto view = m_activeScene->view<Core::ScriptsComponent>();
			for (auto handle : view)
			{
				auto& sc = view.get<Core::ScriptsComponent>(handle);
				for (auto& s : sc.scripts)
				{
					if (s.isEnabled && s.isStarted && s.instance)
					{
						s.instance->onFixedUpdate(dt);
						s.instance->onUpdate(dt);
					}
				}
			}
			Core::Log::Get().info("Editor", "Stepped 1 frame (1/60s).");
		}
	}

	void EditorLayer::onSimulate()
	{
		if (m_editorMode == EditorMode::Simulate) return;

		m_editScene = m_activeScene;
		m_activeScene = Core::Scene::copy(*m_editScene);

		m_editorMode = EditorMode::Simulate;
		m_playState = PlayState::Playing;

		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
		updateWindowTitle();
		Core::Log::Get().info("Editor", "Simulate mode started (physics only).");
	}

	void EditorLayer::onStop()
	{
		if (m_editorMode == EditorMode::Edit) return;

		// Destroy active scripts before restoring edit scene
		if (m_activeScene)
		{
			auto view = m_activeScene->view<Core::ScriptsComponent>();
			for (auto handle : view)
			{
				auto& sc = view.get<Core::ScriptsComponent>(handle);
				for (auto& s : sc.scripts)
				{
					if (s.instance && s.isStarted)
					{
						s.instance->onDestroy();
						s.isStarted = false;
					}
				}
			}
		}

		m_editorMode = EditorMode::Edit;
		m_playState = PlayState::Stopped;

		m_activeScene = m_editScene;
		m_editScene.reset();

		m_selectionContext.clear();
		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);

		updateWindowTitle();
		Core::Log::Get().info("Editor", "Stopped Play/Simulate mode. Restored edit scene.");
	}

	void EditorLayer::onPlayStandalone()
	{
		std::string configName = "Debug";
#if defined(DF_RELEASE)
		configName = "Release";
#elif defined(DF_DIST)
		configName = "Dist";
#endif

		std::filesystem::path playerExe = "bin/" + configName + "-windows-x86_64/Player/Player.exe";
		if (!std::filesystem::exists(playerExe))
		{
			if (std::filesystem::exists("bin/Release-windows-x86_64/Player/Player.exe"))
			{
				playerExe = "bin/Release-windows-x86_64/Player/Player.exe";
			}
			else if (std::filesystem::exists("bin/Debug-windows-x86_64/Player/Player.exe"))
			{
				playerExe = "bin/Debug-windows-x86_64/Player/Player.exe";
			}
		}

		std::string projArg = m_currentProjectPath.empty() ? "Templates/Playground3D/Project.dfproj" : m_currentProjectPath.string();
		std::string cmd = playerExe.string() + " --project \"" + projArg + "\"";
		Core::Log::Get().info("Editor", "Launching standalone Player: {}", cmd);
		WinExec(cmd.c_str(), SW_SHOW);
	}

	void EditorLayer::performAutosave()
	{
		if (!m_activeScene || !m_isSceneDirty) return;

		std::filesystem::path autosavePath;
		if (!m_currentScenePath.empty())
		{
			autosavePath = m_currentScenePath;
			autosavePath.replace_extension(".autosave.scene");
		}
		else
		{
			autosavePath = "Untitled.autosave.scene";
		}

		Core::SceneSerializer serializer(m_activeScene.get());
		if (serializer.serializeToFile(autosavePath))
		{
			Core::Log::Get().info("Editor", "Autosaved scene to: {}", autosavePath.string());
		}
	}

	void EditorLayer::updateWindowTitle()
	{
		if (m_inProjectManager)
		{
			Runtime::Application::Get().getWindow().setTitle("DarkFire Engine 2.0 - Project Manager");
			return;
		}

		std::string projName = m_projectConfig.name.empty() ? "Project" : m_projectConfig.name;
		std::string sceneName = m_currentScenePath.empty() ? "Untitled" : m_currentScenePath.filename().string();
		std::string stateSuffix;

		if (m_editorMode == EditorMode::Play)
		{
			stateSuffix = (m_playState == PlayState::Paused) ? " [PLAYING - PAUSED]" : " [PLAYING]";
		}
		else if (m_editorMode == EditorMode::Simulate)
		{
			stateSuffix = " [SIMULATING]";
		}

		std::string dirtyMark = m_isSceneDirty ? "*" : "";
		std::string title = "DarkFire Engine - " + projName + " - " + sceneName + dirtyMark + stateSuffix;
		Runtime::Application::Get().getWindow().setTitle(title);
	}
}
