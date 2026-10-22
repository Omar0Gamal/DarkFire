#include "Engine/Runtime/Config.h"
#include "Engine/Core/Log/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace Runtime
{
	ProjectConfig::ProjectConfig()
	{
		// Default: all 16 layers collide with all other layers
		collisionMatrix.fill(0xFFFF);
	}

	Core::Result<ProjectConfig, std::string> ProjectConfig::Load(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			return Core::Result<ProjectConfig, std::string>::Err("Failed to open project file: " + path.string());
		}

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		return LoadFromString(content);
	}

	Core::Result<ProjectConfig, std::string> ProjectConfig::LoadFromString(const std::string& jsonContent)
	{
		try
		{
			nlohmann::json j = nlohmann::json::parse(jsonContent);

			ProjectConfig config;

			if (j.contains("Project"))
			{
				const auto& p = j["Project"];
				if (p.contains("Name")) config.name = p["Name"].get<std::string>();
				if (p.contains("Version")) config.version = p["Version"].get<std::string>();
				if (p.contains("EngineVersion")) config.engineVersion = p["EngineVersion"].get<std::string>();
				if (p.contains("AssetDirectory")) config.assetDirectory = p["AssetDirectory"].get<std::string>();
				if (p.contains("StartScene")) config.startScene = p["StartScene"].get<std::string>();
				if (p.contains("Module")) config.modulePath = p["Module"].get<std::string>();
			}

			if (j.contains("Window"))
			{
				const auto& w = j["Window"];
				if (w.contains("Title")) config.windowTitle = w["Title"].get<std::string>();
				else config.windowTitle = config.name;
				if (w.contains("Width")) config.windowWidth = w["Width"].get<uint32_t>();
				if (w.contains("Height")) config.windowHeight = w["Height"].get<uint32_t>();
				if (w.contains("VSync")) config.vsync = w["VSync"].get<bool>();
				if (w.contains("Fullscreen")) config.fullscreen = w["Fullscreen"].get<bool>();
			}
			else
			{
				config.windowTitle = config.name;
			}

			if (j.contains("Physics"))
			{
				const auto& phys = j["Physics"];
				if (phys.contains("Gravity") && phys["Gravity"].is_array() && phys["Gravity"].size() == 3)
				{
					config.gravity = Vector3(
						phys["Gravity"][0].get<float>(),
						phys["Gravity"][1].get<float>(),
						phys["Gravity"][2].get<float>()
					);
				}
				if (phys.contains("FixedTimeStep"))
				{
					config.fixedTimeStep = phys["FixedTimeStep"].get<float>();
				}
				if (phys.contains("CollisionMatrix") && phys["CollisionMatrix"].is_array())
				{
					size_t count = std::min(config.collisionMatrix.size(), phys["CollisionMatrix"].size());
					for (size_t i = 0; i < count; ++i)
					{
						config.collisionMatrix[i] = static_cast<uint16_t>(phys["CollisionMatrix"][i].get<uint32_t>());
					}
				}
			}

			return Core::Result<ProjectConfig, std::string>::Ok(std::move(config));
		}
		catch (const std::exception& e)
		{
			return Core::Result<ProjectConfig, std::string>::Err(std::string("JSON parse error: ") + e.what());
		}
	}

	Core::Result<void, std::string> ProjectConfig::Save(const std::filesystem::path& path) const
	{
		try
		{
			nlohmann::ordered_json j;
			j["Project"]["Name"] = name;
			j["Project"]["Version"] = version;
			j["Project"]["EngineVersion"] = engineVersion;
			j["Project"]["AssetDirectory"] = assetDirectory;
			j["Project"]["StartScene"] = startScene;
			j["Project"]["Module"] = modulePath;

			j["Window"]["Title"] = windowTitle;
			j["Window"]["Width"] = windowWidth;
			j["Window"]["Height"] = windowHeight;
			j["Window"]["VSync"] = vsync;
			j["Window"]["Fullscreen"] = fullscreen;

			j["Physics"]["Gravity"] = { gravity.x, gravity.y, gravity.z };
			j["Physics"]["FixedTimeStep"] = fixedTimeStep;
			std::vector<uint32_t> mat;
			for (auto mask : collisionMatrix)
			{
				mat.push_back(mask);
			}
			j["Physics"]["CollisionMatrix"] = mat;

			std::ofstream file(path);
			if (!file.is_open())
			{
				return Core::Result<void, std::string>::Err("Failed to write to file: " + path.string());
			}

			file << j.dump(2);
			return Core::Result<void, std::string>::Ok();
		}
		catch (const std::exception& e)
		{
			return Core::Result<void, std::string>::Err(std::string("JSON write error: ") + e.what());
		}
	}
}
