#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <filesystem>
#include <array>

namespace Editor
{
	class ProjectSettingsPanel
	{
	public:
		ProjectSettingsPanel();

		void open(const std::filesystem::path& projectPath);
		void draw(bool* p_open = nullptr);
		bool isOpen() const noexcept { return m_isOpen; }

		bool loadFromFile(const std::filesystem::path& path);
		bool saveToFile(const std::filesystem::path& path);

	private:
		bool m_isOpen = false;
		std::filesystem::path m_projectPath;

		// General
		std::string m_projectName;
		std::string m_version = "1.0.0";
		std::string m_engineVersion = "2.0.0";
		std::string m_assetDir = "Assets";
		std::string m_startScene = "Scenes/Playground.scene";
		std::string m_modulePath;

		// Rendering / Window
		std::string m_windowTitle;
		int m_windowWidth = 1280;
		int m_windowHeight = 720;
		bool m_vsync = true;
		bool m_fullscreen = false;

		// Physics
		float m_gravity[3] = { 0.0f, -9.81f, 0.0f };
		float m_fixedTimeStep = 1.0f / 60.0f;
		std::array<uint16_t, 16> m_collisionMatrix{};

		// Networking
		int m_tickRate = 60;
		int m_broadcastRate = 30;
		float m_simulatedLatency = 0.0f;
		float m_simulatedLoss = 0.0f;

		// Audio
		float m_masterVolume = 1.0f;
		float m_sfxVolume = 1.0f;
		float m_musicVolume = 1.0f;
		float m_dopplerFactor = 1.0f;
	};
}
