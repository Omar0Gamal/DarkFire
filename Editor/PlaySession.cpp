#include "Editor/PlaySession.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Log/Log.h"

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
				if (s.isEnabled && s.instance && !s.isStarted)
				{
					s.instance->onStart();
					s.isStarted = true;
				}
			}
		}

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
		}
	}
}
