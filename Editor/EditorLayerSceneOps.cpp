#include "Editor/EditorLayer.h"
#include "Editor/FileDialogs.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Core/Assets/AssetManager.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Log/Log.h"

#include <filesystem>
#include <cstdlib>
#include <string>

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
		m_worldSettingsPanel.setScene(m_activeScene);

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
			m_worldSettingsPanel.setScene(m_activeScene);
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

	void EditorLayer::onPlay()
	{
		m_playSession.onPlay(m_activeScene, [this]() {
			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			m_inspectorPanel.setContext(m_activeScene);
			m_worldSettingsPanel.setScene(m_activeScene);
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
			m_worldSettingsPanel.setScene(m_activeScene);
			updateWindowTitle();
		});
	}

	void EditorLayer::onStop()
	{
		m_playSession.onStop(m_activeScene, [this]() {
			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			m_inspectorPanel.setContext(m_activeScene);
			m_worldSettingsPanel.setScene(m_activeScene);
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

	void EditorLayer::buildAndReloadGameModule()
	{
		Core::Log::Get().info("Editor", "Starting C++ Project Build & Hot-Reload (F7)...");

		// 1. Unload existing Game Module safely
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
						if (s.instance)
						{
							s.serializedFieldsJson = s.instance->serializeExposedFields();
							if (s.isStarted)
							{
								s.instance->onDestroy();
							}
							s.instance.reset();
						}
					}
				}
			}
			m_gameModule->unload();
			m_gameModule.reset();
		}

		// 2. Invoke Build script synchronously
		std::string configName = "Debug";
#if defined(DF_RELEASE)
		configName = "Release";
#elif defined(DF_DIST)
		configName = "Dist";
#endif

		std::string buildCmd = "cmd.exe /c Scripts\\Build.bat " + configName;
		Core::Log::Get().info("Editor", "Executing: {}", buildCmd);
		int exitCode = std::system(buildCmd.c_str());
		if (exitCode != 0)
		{
			Core::Log::Get().error("Editor", "Build failed with exit code: {}", exitCode);
			return;
		}

		// 3. Reload Game Module
		std::filesystem::path projectDir = m_currentProjectPath.empty() ? std::filesystem::current_path() : m_currentProjectPath.parent_path();
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
			auto& ctx = Core::EngineContext::Get();
			auto modRes = Runtime::GameModuleLoader::Load(modPath, ctx);
			if (modRes.isOk())
			{
				m_gameModule = std::make_unique<Runtime::GameModuleLoader>(std::move(modRes.value()));
				Core::Log::Get().info("Editor", "Hot-reloaded Module '{}' successfully! ({} scripts registered)",
					m_gameModule->getInfo().name, Core::ScriptRegistry::Get().getRegisteredNames().size());

				// 4. Re-bind instances on scene
				if (m_activeScene)
				{
					auto view = m_activeScene->view<Core::ScriptsComponent>();
					for (auto e : view)
					{
						auto& sc = view.get<Core::ScriptsComponent>(e);
						for (auto& s : sc.scripts)
						{
							s.instance = Core::ScriptRegistry::Get().instantiate(s.scriptName);
							if (s.instance)
							{
								s.instance->bind(Core::Entity(e, m_activeScene.get()), m_activeScene.get());
								s.instance->onCreate();
								if (!s.serializedFieldsJson.empty())
								{
									s.instance->deserializeExposedFields(s.serializedFieldsJson);
								}
							}
						}
					}
				}
			}
			else
			{
				Core::Log::Get().error("Editor", "Failed to hot-reload Game Module: {}", modRes.error());
			}
		}
	}
}
