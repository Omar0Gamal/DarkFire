#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <filesystem>

namespace Core
{
	class Scene;

	class DF_API SceneSerializer
	{
	public:
		explicit SceneSerializer(Scene* scene);

		// Serialize to JSON string or atomic file write
		std::string serializeToString() const;
		bool serializeToFile(const std::filesystem::path& filepath) const;

		// Deserialize from JSON string or file
		bool deserializeFromString(const std::string& jsonString);
		bool deserializeFromFile(const std::filesystem::path& filepath);

	private:
		Scene* m_scene = nullptr;
	};
}
