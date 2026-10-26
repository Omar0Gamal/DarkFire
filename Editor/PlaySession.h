#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include <memory>
#include <functional>
#include <vector>
#include <filesystem>

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

	enum class PIEMode : uint8_t
	{
		SinglePlayer = 0,
		ServerAnd1Client,
		ServerAnd2Clients
	};

	class PlaySession
	{
	public:
		PlaySession() = default;

		EditorMode getEditorMode() const noexcept { return m_editorMode; }
		PlayState getPlayState() const noexcept { return m_playState; }
		bool isPlaying() const noexcept { return m_playState == PlayState::Playing; }
		bool isPaused() const noexcept { return m_playState == PlayState::Paused; }
		bool isStopped() const noexcept { return m_playState == PlayState::Stopped; }

		void setMode(EditorMode mode) noexcept { m_editorMode = mode; }

		// State transitions
		void onPlay(std::shared_ptr<Core::Scene>& activeScene, std::function<void()> onSceneChanged);
		void onPause(std::function<void()> onStateChanged = nullptr);
		void onStep(std::shared_ptr<Core::Scene>& activeScene);
		void onSimulate(std::shared_ptr<Core::Scene>& activeScene, std::function<void()> onSceneChanged);
		void onStop(std::shared_ptr<Core::Scene>& activeScene, std::function<void()> onSceneChanged);

		// Multi-Instance PIE Launcher (Milestone 8.3)
		void launchMultiInstance(const std::filesystem::path& projectPath, PIEMode mode);
		void stopMultiInstances();
		bool hasRunningInstances() const;

		// Simulation ticks
		void onFixedUpdate(std::shared_ptr<Core::Scene>& activeScene, float fixedDeltaTime);
		void onUpdate(std::shared_ptr<Core::Scene>& activeScene, float deltaTime);

	private:
		EditorMode m_editorMode = EditorMode::Edit;
		PlayState m_playState = PlayState::Stopped;
		std::shared_ptr<Core::Scene> m_editScene;

		std::vector<void*> m_spawnedProcessHandles;
	};
}
