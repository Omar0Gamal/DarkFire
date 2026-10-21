#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Entity.h"
#include <string>
#include <vector>
#include <span>
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

		// Entity-level serialization for commands, undo/redo, and clipboard
		std::string serializeEntity(Entity entity) const;
		Entity deserializeEntity(const std::string& jsonString);

		std::string serializeEntities(std::span<const Entity> entities) const;
		std::vector<Entity> deserializeEntities(const std::string& jsonString, bool remapUuids = false, const Vector3& positionOffset = Vector3(0.0f, 0.0f, 0.0f));

	private:
		Scene* m_scene = nullptr;
	};
}
