#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include "Engine/Core/Math/Math.h"
#include <string>
#include <array>
#include <filesystem>

namespace Runtime
{
	using Core::Vector3;

	struct DF_API ProjectConfig
	{
		std::string name = "DarkFireProject";
		std::string version = "1.0.0";
		std::string engineVersion = "2.0.0";
		std::string assetDirectory = "Assets";
		std::string startScene = "Scenes/Main.scene";
		std::string modulePath = "";

		std::string windowTitle = "DarkFire Engine";
		uint32_t windowWidth = 1280;
		uint32_t windowHeight = 720;
		bool vsync = true;
		bool fullscreen = false;
		bool enableImGui = false;
		bool headless = false;

		Vector3 gravity = Vector3(0.0f, -9.81f, 0.0f);
		float fixedTimeStep = 1.0f / 60.0f;
		std::array<uint16_t, 16> collisionMatrix{};

		ProjectConfig();

		static Core::Result<ProjectConfig, std::string> Load(const std::filesystem::path& path);
		static Core::Result<ProjectConfig, std::string> LoadFromString(const std::string& jsonContent);
		Core::Result<void, std::string> Save(const std::filesystem::path& path) const;
	};
}
