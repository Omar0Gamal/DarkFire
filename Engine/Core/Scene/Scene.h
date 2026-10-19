#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/Entity.h"
#include <entt/entt.hpp>
#include <unordered_map>
#include <string_view>
#include <memory>
#include <vector>

namespace Core
{
	class DF_API Scene
	{
	public:
		Scene();
		~Scene();

		// Entity Lifecycle
		Entity createEntity(const std::string& name = "Entity");
		Entity createEntityWithUUID(UUID uuid, const std::string& name = "Entity");
		void destroyEntity(Entity entity);

		// Queries
		Entity findEntityByUUID(UUID uuid) const;
		Entity findEntityByName(std::string_view name) const;
		bool containsEntity(UUID uuid) const;
		size_t getEntityCount() const noexcept { return m_entityMap.size(); }

		// Hierarchy & Transforms
		void reparent(Entity entity, Entity newParent, bool keepWorldTransform = true);
		void updateTransforms();

		// Deep Copy
		static std::unique_ptr<Scene> copy(const Scene& src);

		// Registry Access
		entt::registry& getRegistry() noexcept { return m_registry; }
		const entt::registry& getRegistry() const noexcept { return m_registry; }

		template<typename... Components>
		auto view() { return m_registry.view<Components...>(); }

		template<typename... Components>
		auto view() const { return m_registry.view<Components...>(); }

	private:
		void updateTransformRecursive(entt::entity e, const Matrix& parentWorld, bool parentDirty);

	private:
		entt::registry m_registry;
		std::unordered_map<UUID, entt::entity> m_entityMap;

		friend class Entity;
		friend class SceneSerializer;
	};

	// Template implementations for Entity
	template<typename T, typename... Args>
	inline T& Entity::addComponent(Args&&... args)
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		DF_ASSERT(!hasComponent<T>(), "Entity already has component!");
		return m_scene->m_registry.emplace<T>(m_handle, std::forward<Args>(args)...);
	}

	template<typename T>
	inline T& Entity::getComponent()
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		DF_ASSERT(hasComponent<T>(), "Entity does not have component!");
		return m_scene->m_registry.get<T>(m_handle);
	}

	template<typename T>
	inline const T& Entity::getComponent() const
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		DF_ASSERT(hasComponent<T>(), "Entity does not have component!");
		return m_scene->m_registry.get<T>(m_handle);
	}

	template<typename T>
	inline T* Entity::tryGetComponent()
	{
		if (!isValid()) return nullptr;
		return m_scene->m_registry.try_get<T>(m_handle);
	}

	template<typename T>
	inline const T* Entity::tryGetComponent() const
	{
		if (!isValid()) return nullptr;
		return m_scene->m_registry.try_get<T>(m_handle);
	}

	template<typename T>
	inline bool Entity::hasComponent() const
	{
		if (!isValid()) return false;
		return m_scene->m_registry.all_of<T>(m_handle);
	}

	template<typename T>
	inline void Entity::removeComponent()
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		DF_ASSERT(hasComponent<T>(), "Entity does not have component!");
		m_scene->m_registry.remove<T>(m_handle);
	}
}
