#include "SceneSerializer.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Reflection/Reflection.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <algorithm>

namespace Core
{
	SceneSerializer::SceneSerializer(Scene* scene)
		: m_scene(scene)
	{
		DF_ASSERT(m_scene != nullptr, "Scene cannot be null in SceneSerializer!");
		ComponentRegistry::Get().registerBuiltinTypes();
	}

	std::string SceneSerializer::serializeToString() const
	{
		nlohmann::ordered_json root;
		root["scene_version"] = 1;

		// Collect all entities and sort deterministically by UUID
		std::vector<UUID> sortedUuids;
		sortedUuids.reserve(m_scene->m_entityMap.size());
		for (const auto& [uuid, handle] : m_scene->m_entityMap)
		{
			sortedUuids.push_back(uuid);
		}
		std::sort(sortedUuids.begin(), sortedUuids.end());

		nlohmann::ordered_json entitiesArray = nlohmann::ordered_json::array();
		const auto& registry = ComponentRegistry::Get();
		const auto& allComponents = registry.getAll();

		for (UUID uuid : sortedUuids)
		{
			entt::entity handle = m_scene->m_entityMap.at(uuid);
			nlohmann::ordered_json entityNode;

			const auto& idComp = m_scene->m_registry.get<IdentityComponent>(handle);
			entityNode["uuid"] = idComp.id.value();
			entityNode["name"] = idComp.name;
			entityNode["tag"] = idComp.tag;

			const auto& hComp = m_scene->m_registry.get<HierarchyComponent>(handle);
			if (hComp.parent != entt::null && m_scene->m_registry.valid(hComp.parent))
			{
				entityNode["parent"] = m_scene->m_registry.get<IdentityComponent>(hComp.parent).id.value();
			}
			else
			{
				entityNode["parent"] = 0;
			}

			nlohmann::ordered_json componentsNode = nlohmann::ordered_json::object();
			for (const auto& desc : allComponents)
			{
				// Skip IdentityComponent since it's already top-level; only authored components
				if (desc.isAuthored && desc.name != "IdentityComponent" && desc.hasComponent(*m_scene, handle))
				{
					nlohmann::ordered_json compNode;
					desc.serializeToJson(*m_scene, handle, &compNode);
					componentsNode[desc.name] = compNode;
				}
			}

			entityNode["components"] = componentsNode;
			entitiesArray.push_back(entityNode);
		}

		root["entities"] = entitiesArray;
		return root.dump(4);
	}

	bool SceneSerializer::serializeToFile(const std::filesystem::path& filepath) const
	{
		std::string jsonStr = serializeToString();
		std::filesystem::path tempPath = filepath;
		tempPath += ".tmp";

		if (filepath.has_parent_path())
		{
			std::filesystem::create_directories(filepath.parent_path());
		}

		{
			std::ofstream out(tempPath, std::ios::out | std::ios::trunc);
			if (!out.is_open()) return false;
			out << jsonStr;
			out.flush();
		}

		std::error_code ec;
		std::filesystem::rename(tempPath, filepath, ec);
		if (ec)
		{
			std::filesystem::copy_file(tempPath, filepath, std::filesystem::copy_options::overwrite_existing, ec);
			std::filesystem::remove(tempPath, ec);
		}
		return !ec;
	}

	bool SceneSerializer::deserializeFromString(const std::string& jsonString)
	{
		nlohmann::ordered_json root;
		try
		{
			root = nlohmann::ordered_json::parse(jsonString);
		}
		catch (...)
		{
			return false;
		}

		if (!root.contains("scene_version") || !root.contains("entities"))
		{
			return false;
		}

		// Clear existing scene
		m_scene->m_entityMap.clear();
		m_scene->m_registry.clear();

		const auto& registry = ComponentRegistry::Get();
		const auto& entitiesArray = root["entities"];

		struct EntityParentPair
		{
			Entity entity;
			uint64_t parentUuid = 0;
		};
		std::vector<EntityParentPair> hierarchyPairs;
		hierarchyPairs.reserve(entitiesArray.size());

		// 1. First pass: create all entities with UUIDs, names, tags
		for (const auto& entityNode : entitiesArray)
		{
			uint64_t uuidVal = entityNode["uuid"].get<uint64_t>();
			std::string name = entityNode.value("name", "Entity");
			std::string tag = entityNode.value("tag", "Default");

			Entity entity = m_scene->createEntityWithUUID(UUID(uuidVal), name);
			entity.getComponent<IdentityComponent>().tag = tag;

			uint64_t parentUuid = entityNode.value("parent", (uint64_t)0);
			if (parentUuid != 0)
			{
				hierarchyPairs.push_back({ entity, parentUuid });
			}

			// Deserialize components
			if (entityNode.contains("components"))
			{
				const auto& componentsNode = entityNode["components"];
				for (auto it = componentsNode.begin(); it != componentsNode.end(); ++it)
				{
					const std::string& compName = it.key();
					const auto* desc = registry.findByName(compName);
					if (desc && desc->deserializeFromJson)
					{
						desc->deserializeFromJson(*m_scene, entity.getHandle(), &it.value());
					}
				}
			}
		}

		// 2. Second pass: link hierarchy
		for (const auto& pair : hierarchyPairs)
		{
			Entity parent = m_scene->findEntityByUUID(UUID(pair.parentUuid));
			if (parent)
			{
				m_scene->reparent(pair.entity, parent, false);
			}
		}

		m_scene->updateTransforms();
		return true;
	}

	bool SceneSerializer::deserializeFromFile(const std::filesystem::path& filepath)
	{
		std::ifstream in(filepath);
		if (!in.is_open()) return false;

		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return deserializeFromString(content);
	}
}
