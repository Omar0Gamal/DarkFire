#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Components.h"
#include <entt/entt.hpp>
#include <vector>
#include <string>

namespace Core
{
	class Scene;

	class DF_API Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: m_handle(handle), m_scene(scene) {}
		Entity(const Entity&) = default;
		Entity& operator=(const Entity&) = default;

		template<typename T, typename... Args>
		T& addComponent(Args&&... args);

		template<typename T>
		T& getComponent();

		template<typename T>
		const T& getComponent() const;

		template<typename T>
		T* tryGetComponent();

		template<typename T>
		const T* tryGetComponent() const;

		template<typename T>
		bool hasComponent() const;

		template<typename T>
		void removeComponent();

		UUID getUUID() const;
		const std::string& getName() const;

		TransformComponent& getTransform();
		const TransformComponent& getTransform() const;
		Matrix getWorldMatrix() const;

		Entity getParent() const;
		std::vector<Entity> getChildren() const;
		void setParent(Entity newParent, bool keepWorldTransform = true);

		void destroy();

		bool isActive() const;
		void setActive(bool active);

		bool isValid() const noexcept;
		operator bool() const noexcept { return isValid(); }
		operator entt::entity() const noexcept { return m_handle; }
		entt::entity getHandle() const noexcept { return m_handle; }
		Scene* getScene() const noexcept { return m_scene; }

		bool operator==(const Entity& other) const noexcept { return m_handle == other.m_handle && m_scene == other.m_scene; }
		bool operator!=(const Entity& other) const noexcept { return !(*this == other); }

	private:
		entt::entity m_handle{ entt::null };
		Scene* m_scene{ nullptr };
	};
}
