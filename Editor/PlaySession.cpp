#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "Editor/PlaySession.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Audio/AudioSystem.h"

namespace Editor
{
	void PlaySession::onPlay(std::shared_ptr<Core::Scene>& activeScene, std::function<void()> onSceneChanged)
	{
		if (m_playState == PlayState::Playing) return;

		if (m_playState == PlayState::Paused)
		{
			m_playState = PlayState::Playing;
			if (onSceneChanged) onSceneChanged();
			return;
		}

		if (!activeScene) return;

		m_editScene = activeScene;
		activeScene = Core::Scene::copy(*m_editScene);

		m_editorMode = EditorMode::Play;
		m_playState = PlayState::Playing;

		if (onSceneChanged) onSceneChanged();

		// Start all attached scripts
		auto view = activeScene->view<Core::ScriptsComponent>();
		for (auto handle : view)
		{
			auto& sc = view.get<Core::ScriptsComponent>(handle);
			for (auto& s : sc.scripts)
			{
				if (!s.instance && !s.scriptName.empty())
				{
					s.instance = Core::ScriptRegistry::Get().instantiate(s.scriptName);
					if (s.instance)
					{
						s.instance->bind(Core::Entity(handle, activeScene.get()), activeScene.get());
						s.instance->onCreate();
						if (!s.serializedFieldsJson.empty())
						{
							s.instance->deserializeExposedFields(s.serializedFieldsJson);
						}
					}
				}

				if (s.isEnabled && s.instance && !s.isStarted)
				{
					s.instance->onStart();
					s.isStarted = true;
				}
			}
		}

		Audio::AudioSystem::Get().init();
		Core::Log::Get().info("Editor", "Play mode started.");
	}

	void PlaySession::onPause(std::function<void()> onStateChanged)
	{
		if (m_playState == PlayState::Playing)
		{
			m_playState = PlayState::Paused;
			if (onStateChanged) onStateChanged();
			Core::Log::Get().info("Editor", "Play mode paused.");
		}
		else if (m_playState == PlayState::Paused)
		{
			m_playState = PlayState::Playing;
			if (onStateChanged) onStateChanged();
			Core::Log::Get().info("Editor", "Play mode resumed.");
		}
	}

	void PlaySession::onStep(std::shared_ptr<Core::Scene>& activeScene)
	{
		if (m_playState == PlayState::Paused && activeScene)
		{
			float dt = 1.0f / 60.0f;
			Runtime::Application::Get().getPhysicsWorld().step(*activeScene, dt);

			auto view = activeScene->view<Core::ScriptsComponent>();
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

	void PlaySession::onSimulate(std::shared_ptr<Core::Scene>& activeScene, std::function<void()> onSceneChanged)
	{
		if (m_editorMode == EditorMode::Simulate) return;
		if (!activeScene) return;

		m_editScene = activeScene;
		activeScene = Core::Scene::copy(*m_editScene);

		m_editorMode = EditorMode::Simulate;
		m_playState = PlayState::Playing;

		if (onSceneChanged) onSceneChanged();
		Core::Log::Get().info("Editor", "Simulate mode started (physics only).");
	}

	void PlaySession::onStop(std::shared_ptr<Core::Scene>& activeScene, std::function<void()> onSceneChanged)
	{
		if (m_editorMode == EditorMode::Edit) return;

		// Destroy active scripts before restoring edit scene
		if (activeScene)
		{
			auto view = activeScene->view<Core::ScriptsComponent>();
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

		if (m_editScene)
		{
			activeScene = m_editScene;
			m_editScene.reset();
		}

		m_editorMode = EditorMode::Edit;
		m_playState = PlayState::Stopped;

		if (onSceneChanged) onSceneChanged();
		Audio::AudioSystem::Get().shutdown();
		Core::Log::Get().info("Editor", "Simulation stopped. Restored edit scene.");
	}

	void PlaySession::onFixedUpdate(std::shared_ptr<Core::Scene>& activeScene, float fixedDeltaTime)
	{
		if ((m_editorMode == EditorMode::Play || m_editorMode == EditorMode::Simulate) && m_playState == PlayState::Playing)
		{
			if (activeScene)
			{
				Runtime::Application::Get().getPhysicsWorld().step(*activeScene, fixedDeltaTime);

				if (m_editorMode == EditorMode::Play)
				{
					auto view = activeScene->view<Core::ScriptsComponent>();
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

	void PlaySession::onUpdate(std::shared_ptr<Core::Scene>& activeScene, float deltaTime)
	{
		if (m_editorMode == EditorMode::Play && m_playState == PlayState::Playing && activeScene)
		{
			auto view = activeScene->view<Core::ScriptsComponent>();
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

			Audio::AudioSystem::Get().update(*activeScene, deltaTime);
		}
	}


	bool PlaySession::hasRunningInstances() const
	{
		for (void* h : m_spawnedProcessHandles)
		{
			DWORD exitCode = 0;
			if (GetExitCodeProcess(static_cast<HANDLE>(h), &exitCode) && exitCode == STILL_ACTIVE)
			{
				return true;
			}
		}
		return false;
	}

	void PlaySession::stopMultiInstances()
	{
		for (void* h : m_spawnedProcessHandles)
		{
			HANDLE hProcess = static_cast<HANDLE>(h);
			DWORD exitCode = 0;
			if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE)
			{
				TerminateProcess(hProcess, 0);
			}
			CloseHandle(hProcess);
		}
		m_spawnedProcessHandles.clear();
		Core::Log::Get().info("Editor", "Stopped all multi-instance PIE sessions.");
	}

	void PlaySession::launchMultiInstance(const std::filesystem::path& projectPath, PIEMode mode)
	{
		stopMultiInstances();
		if (mode == PIEMode::SinglePlayer) return;

		std::string playerExe = "bin/Debug-windows-x86_64/Player/Player.exe";
		if (!std::filesystem::exists(playerExe))
		{
			playerExe = "bin/Release-windows-x86_64/Player/Player.exe";
		}
		if (!std::filesystem::exists(playerExe))
		{
			Core::Log::Get().error("Editor", "Cannot launch PIE: Player.exe not found.");
			return;
		}

		auto spawnProcess = [&](const std::string& extraArgs) -> HANDLE {
			std::string cmd = "\"" + playerExe + "\" --project \"" + projectPath.string() + "\" " + extraArgs;
			STARTUPINFOA si{};
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi{};
			if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
			{
				CloseHandle(pi.hThread);
				return pi.hProcess;
			}
			return nullptr;
		};

		if (mode == PIEMode::ServerAnd1Client)
		{
			Core::Log::Get().info("Editor", "Spawning 1 Server + 1 Client PIE session...");
			HANDLE hServer = spawnProcess("--server");
			if (hServer) m_spawnedProcessHandles.push_back(hServer);

			HANDLE hClient = spawnProcess("");
			if (hClient) m_spawnedProcessHandles.push_back(hClient);
		}
		else if (mode == PIEMode::ServerAnd2Clients)
		{
			Core::Log::Get().info("Editor", "Spawning 1 Server + 2 Clients PIE session...");
			HANDLE hServer = spawnProcess("--server");
			if (hServer) m_spawnedProcessHandles.push_back(hServer);

			HANDLE hClient1 = spawnProcess("");
			if (hClient1) m_spawnedProcessHandles.push_back(hClient1);

			HANDLE hClient2 = spawnProcess("");
			if (hClient2) m_spawnedProcessHandles.push_back(hClient2);
		}
	}
}
